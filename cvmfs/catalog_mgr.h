/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_CATALOG_MGR_H_
#define CVMFS_CATALOG_MGR_H_

#include <vector>
#include <string>

#include "catalog.h"
#include "dirent.h"
#include "hash.h"

namespace catalog {

enum LookupOptions {
  kLookupSole = 0,
  kLookupFull,
};

/**
 * This class provides the read-only interface to a tree of catalogs
 * representing a (subtree of a) repository.
 * Mostly lookup functions filling DirectoryEntry objects.
 * Reloading of expired catalogs, attaching of nested catalogs and delegating
 * of lookups to the appropriate catalog is done transparently.
 *
 * The loading / creating of catalogs is up to derived classes.
 *
 * Usage:
 *   DerivedCatalogManager *catalog_manager = new DerivedCatalogManager();
 *   catalog_manager->Init();
 *   catalog_manager->Lookup(<inode>, &<result_entry>);
 */
class CatalogManager {
 public:
  CatalogManager();
  virtual ~CatalogManager();

  virtual bool Init();

  bool LookupInode(const inode_t inode, const LookupOptions options,
                   DirectoryEntry *entry) const;
  bool LookupPath(const std::string &path, const LookupOptions options,
                  DirectoryEntry *entry);
  bool Listing(const std::string &path, DirectoryEntryList *listing);

  /**
   *  get the inode number of the root DirectoryEntry
   *  'root' here means the actual root of the whole file system
   *  @return the root inode number
   */
  inline inode_t GetRootInode() const { return kInitialInodeOffset + 1; }

  /**
   *  get the revision of the catalog
   *  TODO: CURRENTLY NOT IMPLEMENTED
   *  @return the revision number of the catalog
   */
  inline uint64_t GetRevision() const { return 0; } // TODO: implement this

  /**
   *  count all attached catalogs
   *  @return the number of all attached catalogs
   */
  inline int GetNumCatalogs() const { return catalogs_.size(); }

  /**
   *  Inodes are ambiquitous under some circumstances, to prevent problems
   *  they must be passed through this method first
   *  @param inode the raw inode
   *  @return the revised inode
   */
  inline inode_t MangleInode(const inode_t inode) const { return (inode < kInitialInodeOffset) ? GetRootInode() : inode; }

  /**
   *  print the currently attached catalog hierarchy to stdout
   */
  inline void PrintCatalogHierarchy() const { PrintCatalogHierarchyRecursively(GetRootCatalog()); }

 protected:

  // TODO: remove these stubs and replace them with actual locking
  inline void ReadLock() const { }
  inline void WriteLock() const { }
  inline void Unlock() const { }
  inline void UpgradeLock() const { Unlock(); WriteLock(); }
  inline void DowngradeLock() const { Unlock(); ReadLock(); }

  /**
   *  This pure virtual method has to be implemented by deriving classes
   *  It should perform a specific loading action, return 0 on success and
   *  communicate a path to the readily loaded catalog file for attachment.
   *  @param url_path the url path of the catalog to load
   *  @param mount_point the md5 hash of the mount point for this catalog
   *  @param catalog_file must be set to the path of the loaded file
   *  @return 0 on success otherwise a application specific error code
   */
  virtual int LoadCatalogFile(const std::string &url_path, const hash::Md5 &mount_point,
                              std::string *catalog_file) = 0;

  /**
   *  Pure virtual method to create a new catalog structure
   *  Under different circumstances we might need different types of catalog
   *  structures. Every derived class has to implement this and return a newly
   *  created (derived) Catalog structure of it's desired type.
   *  @param mountpoint the future mountpoint of the catalog to create
   *  @param parent_catalog the parent of the catalog to create
   *  @return a newly created (derived) Catalog for future usage
   */
  virtual Catalog* CreateCatalogStub(const std::string &mountpoint,
                                     Catalog *parent_catalog) const = 0;

  /**
   *  loads a new catalog and attaches it on this CatalogManager
   *  for loading of the catalog the pure virtual method LoadCatalogFile is used.
   *  @param mountpoint the mount point path of the catalog to load/attach
   *  @param parent_catalog the direct parent of the catalog to load
   *  @param attached_catalog will be set to the newly attached catalog
   *  @return true on success otherwise false
   */
  bool LoadAndAttachCatalog(const std::string &mountpoint,
                            Catalog *parent_catalog,
                            Catalog **attached_catalog = NULL);

  /**
   *  Attaches all catalogs of the repository recursively
   *  This is useful when updating a repository on the server.
   *  Be careful when using it on remote catalogs
   *  @return true on success, false otherwise
   */
  inline bool LoadAndAttachCatalogsRecursively() { return LoadAndAttachCatalogsRecursively(GetRootCatalog()); }

  /**
   *  attaches a newly created catalog
   *  @param db_file the file on a local file system containing the database
   *  @param new_catalog the catalog to attach to this CatalogManager
   *  @param open_transaction ????
   *  @return true on success, false otherwise
   */
  bool AttachCatalog(const std::string &db_file,
                     Catalog *new_catalog,
                     const bool open_transaction);

  /**
   *  removes a catalog (and all of it's children) from this CatalogManager
   *  the given catalog and all children are freed, if this call succeeds!
   *  @param catalog the catalog to detach
   *  @return true on success, false otherwise
   */
  bool DetachCatalogTree(Catalog *catalog);

   /**
    *  removes a catalog from this CatalogManager
    *  the given catalog pointer is freed if the call succeeds!
    *  CAUTION: This method can create dangling children.
    *           use DetachCatalogTree() if you are unsure!
    *  @param catalog the catalog to detach
    *  @return true on success, false otherwise
    */
   bool DetachCatalog(Catalog *catalog);

  /**
   *  detach all catalogs from this CatalogManager
   *  this is mainly called in the destructor of this class
   *  @return true on success, false otherwise
   */
  inline bool DetachAllCatalogs() { return DetachCatalogTree(GetRootCatalog()); }

  /**
   *  get the root catalog of this CatalogManager
   *  @return the root catalog of this CatalogMananger
   */
  inline Catalog* GetRootCatalog() const { return catalogs_.front(); }

  /**
   *  find the appropriate catalog for a given path.
   *  this method might load additional nested catalogs.
   *  @param path the path for which the catalog is needed
   *  @param load_final_catalog if the last part of the given path is a nested catalog
   *                            it is loaded as well, otherwise not (i.e. directory listing)
   *  @param catalog this pointer will be set to the searched catalog
   *  @param entry if a DirectoryEntry pointer is given, it will be set to the
   *               DirectoryEntry representing the last part of the given path
   *  @return true if catalog was found, false otherwise
   */
  bool GetCatalogByPath(const std::string &path,
                        const bool load_final_catalog,
                        Catalog **catalog = NULL,
                        DirectoryEntry *entry = NULL);

  /**
   *  finds the appropriate catalog for a given inode
   *  Note: This method will NOT load additional nested catalogs
   *  it will match the inode to a allocated inode range.
   *  @param inode the inode to find the associated catalog for
   *  @param catalog this pointer will be set to the result catalog
   *  @return true if catalog was present, false otherwise
   */
  bool GetCatalogByInode(const inode_t inode, Catalog **catalog) const;

  /**
   *  checks if a searched catalog is already present in this CatalogManager
   *  based on it's path.
   *  @param root_path the root path of the searched catalog
   *  @param attached_catalog is set to the searched catalog, in case of presence
   *  @return true if catalog is already present, false otherwise
   */
  bool IsCatalogAttached(const std::string &root_path,
                         Catalog **attached_catalog) const;

 private:
  Catalog* WalkTree(const std::string &path) const;
  bool MountSubtree(const std::string &path, const Catalog *entry_point,
                    Catalog **leaf_catalog);

  /**
   *  this method loads all nested catalogs neccessary to serve a certain path
   *  @param path the path to load the associated nested catalog for
   *  @param entry_point one can specify the catalog to start the search at
   *                     (i.e. the result of FindBestFittingCatalogForPath)
   *  @param load_final_catalog if the last part of path is a nested catalog
   *                            it will be loaded as well, otherwise not
   *  @param final_catalog this will be set to the resulting catalog
   *  @return true if desired nested catalog was successfully loaded, false otherwise
   */
  bool LoadNestedCatalogForPath(const std::string &path,
                                const Catalog *entry_point,
                                const bool load_final_catalog,
                                Catalog **final_catalog);

  /**
   *  prints all attached catalogs to stdout
   *  this is mainly for debugging purposes!
   *  @param catalog the catalog to traverse in this recursion step
   *  @param recursion_depth to determine the indentation
   */
  void PrintCatalogHierarchyRecursively(const Catalog *catalog,
                                        const int recursion_depth = 0) const;

  /**
   *  allocate a chunk of inodes for the given size
   *  this is done while attaching a new catalog
   *  @param size the number of inodes needed
   *  @return a structure defining a chunk of inodes to use for this catalog
   */
  InodeRange GetInodeChunkOfSize(uint64_t size);

  /**
   *  this method is called if a catalog is detached
   *  which renders the associated InodeChunk invalid
   *  here you can clean caches or do some other fancy stuff
   *  @param chunk the InodeChunk to be freed
   */
  void AnnounceInvalidInodeChunk(const InodeRange chunk) const;

  /**
   *  Attaches all catalogs of the repository recursively
   *  This is useful when updating a repository on the server.
   *  Be careful when using it on remote catalogs.
   *  (This is the actual recursion, there is a convenience wrapper in the
   *   protected part of this class)
   *  @param catalog the catalog whose children are attached in this recursion step
   *  @return true on success, false otherwise
   */
  bool LoadAndAttachCatalogsRecursively(Catalog *catalog);

 private:
  // TODO: this list is actually not really needed.
  //       the only point we are really using it at the moment
  //       is for searching the suited catalog for a given inode.
  //       this might be done better (currently O(n))
  //  eventually we should only safe the root catalog, representing
  //  the whole catalog tree in an implicit manor.
  CatalogList catalogs_;

  uint64_t current_inode_offset_;

  const static inode_t kInitialInodeOffset = 255;
};

}

#endif  // CVMFS_CATALOG_MGR_H_