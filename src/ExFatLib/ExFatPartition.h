/* ExFat Library
 * Copyright (C) 2016..2017 by William Greiman
 *
 * This file is part of the ExFat Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ExFat Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef ExFatPartition_h
#define ExFatPartition_h
/**
 * \file
 * \brief ExFatPartition include file.
 */
#include "SysCall.h"
#include "BlockDevice.h"
#include "ExFatConfig.h"
#include "ExFatTypes.h"
/** Type for exFAT partition */
const uint8_t EXFAT_TYPE = 64;

class ExFatFile;
//==============================================================================
/**
 * \class FsCache
 * \brief Sector cache.
 */
class FsCache {
 public:
  /** Cached sector is dirty */
  static const uint8_t CACHE_STATUS_DIRTY = 1;
  /** Cache sector status bits */
  static const uint8_t CACHE_STATUS_MASK = CACHE_STATUS_DIRTY;
  /** Sync existing sector but do not read new sector. */
  static const uint8_t CACHE_OPTION_NO_READ = 2;
  /** Cache sector for read. */
  static const uint8_t CACHE_FOR_READ = 0;
  /** Cache sector for write. */
  static const uint8_t CACHE_FOR_WRITE = CACHE_STATUS_DIRTY;
  /** Reserve cache sector for write - do not read from sector device. */
  static const uint8_t CACHE_RESERVE_FOR_WRITE
    = CACHE_STATUS_DIRTY | CACHE_OPTION_NO_READ;

  FsCache() : m_blockDev(nullptr) {
    invalidate();
  }

  /** \return Cache sector address. */
  uint8_t* cacheBuffer() {
    return m_cacheBuffer;
  }
  /** \return Clear the cache and returns a pointer to the cache. */
  uint8_t* clear() {
    if (isDirty() && !sync()) {
      return nullptr;
    }
    invalidate();
    return m_cacheBuffer;
  }
  /** Set current sector dirty. */
  void dirty() {
    m_status |= CACHE_STATUS_DIRTY;
  }
  /** Initialize the cache.
   * \param[in] blockDev Block device for this partition.
   */
  void init(BlockDevice* blockDev) {
    m_blockDev = blockDev;
    invalidate();
  }
  /** Invalidate current cache sector. */
  void invalidate();
  /** \return dirty status */
  bool isDirty() {
    return m_status & CACHE_STATUS_DIRTY;
  }
  /** \return Logical sector number for cached sector. */
  uint32_t sector() {
    return m_sector;
  }
  /** Fill cache with sector data.
   * \param[in] sector Sector to read.
   * \param[in] option mode for cached sector.
   * \return Address of cached sector. */
  uint8_t* get(uint32_t sector, uint8_t option);
  /** Write current sector if dirty.
   * \return true for success else false.
   */
  bool sync();

 private:
  uint8_t m_status;
  BlockDevice* m_blockDev;
  uint32_t m_sector;
  uint8_t m_cacheBuffer[512];
};
//=============================================================================
/**
 * \class ExFatPartition
 * \brief Access exFat partitions on raw file devices.
 */
class ExFatPartition {
 public:
  ExFatPartition() : m_fatType(0) {}
  /** \return the number of bytes in a cluster. */
  uint32_t bytesPerCluster() {return m_bytesPerCluster;}
  /** \return the power of two for bytesPerCluster. */
  uint8_t bytesPerClusterShift() {
    return m_bytesPerSectorShift + m_sectorsPerClusterShift;
  }
  /** \return the number of bytes in a sector. */
  uint16_t bytesPerSector() {return m_bytesPerSector;}
  /** \return the power of two for bytesPerSector. */
  uint8_t bytesPerSectorShift() {return m_bytesPerSectorShift;}

  /** Clear the cache and returns a pointer to the cache.  Not for normal apps.
   * \return A pointer to the cache buffer or zero if an error occurs.
   */
  uint8_t* cacheClear() {
    return m_dataCache.clear();
  }
  /** \return the cluster count for the partition. */
  uint32_t clusterCount() {return m_clusterCount;}
  /** \return the cluster heap start sector. */
  uint32_t clusterHeapStartSector() {return m_clusterHeapStartSector;}
  /** \return the FAT length in sectors */
  uint32_t fatLength() {return m_fatLength;}
  /** \return the FAT start sector number. */
  uint32_t fatStartSector() {return m_fatStartSector;}
  /** \return Type for exFAT partition */
  uint8_t fatType() const {return m_fatType;}
  /** \return the free cluster count. */
  uint32_t freeClusterCount();
  /** Initialize a exFAT partition.
   * \param[in] dev The blockDevice for the partition.
   * \param[in] part The partition to be used.  Legal values for \a part are
   * 1-4 to use the corresponding partition on a device formatted with
   * a MBR, Master Boot Record, or zero if the device is formatted as
   * a super floppy with the FAT boot sector in sector zero.
   *
   * \return The value true is returned for success and
   * the value false is returned for failure.
   */
  bool init(BlockDevice* dev, uint8_t part);
  /** \return the root directory start cluster number. */
  uint32_t rootDirectoryCluster() {return m_rootDirectoryCluster;}
  /** \return the root directory length. */
  uint32_t rootLength();
  /** \return the number of sectors in a cluster. */
  uint32_t sectorsPerCluster() {return 1 << m_sectorsPerClusterShift;}
  /** \return the power of two for sectors per cluster. */
  uint8_t  sectorsPerClusterShift() {return m_sectorsPerClusterShift;}

  //---------------------------------------------------------------------------
#if ENABLE_ARDUINO_FEATURES
  void checkUpcase(Print* pr);
  bool printDir(Print* pr, ExFatFile* file);
  void dmpBitmap(Print* pr);
  void dmpFat(Print* pr, uint32_t start, uint32_t count);
  void dmpSector(Print* pr, uint32_t sector);
  bool printVolInfo(Print* pr);
  void printFat(Print* pr);
  void printUpcase(Print* pr);
#endif  // #if ENABLE_ARDUINO_FEATURES
  //----------------------------------------------------------------------------
 private:
  friend class ExFatFile;
  uint32_t bitmapFind(uint32_t cluster, uint32_t count);
  bool bitmapModify(uint32_t cluster, uint32_t count, bool value);
  //----------------------------------------------------------------------------
  // Cache functions.
  uint8_t* bitmapCacheGet(uint32_t sector, uint8_t option) {
#if USE_EXFAT_BITMAP_CACHE
    return m_bitmapCache.get(sector, option);
#else  // USE_EXFAT_BITMAP_CACHE
    return m_dataCache.get(sector, option);
#endif  // USE_EXFAT_BITMAP_CACHE
  }
  void cacheInit(BlockDevice* dev) {
#if USE_EXFAT_BITMAP_CACHE
    m_bitmapCache.init(dev);
#endif  // USE_EXFAT_BITMAP_CACHE
    m_dataCache.init(dev);
  }
  bool cacheSync() {
#if USE_EXFAT_BITMAP_CACHE
    return m_bitmapCache.sync() && m_dataCache.sync() && syncDevice();
#else  // USE_EXFAT_BITMAP_CACHE
    return m_dataCache.sync() && syncDevice();
#endif  // USE_EXFAT_BITMAP_CACHE
  }
  void dataCacheDirty() {m_dataCache.dirty();}
  void dataCacheInvalidate() {m_dataCache.invalidate();}
  uint8_t* dataCacheGet(uint32_t sector, uint8_t option) {
    return m_dataCache.get(sector, option);
  }
  uint32_t dataCacheSector() {return m_dataCache.sector();}
  bool dataCacheSync() {return m_dataCache.sync();}
  //----------------------------------------------------------------------------
  uint32_t clusterMask() {return m_clusterMask;}
  uint32_t clusterStartSector(uint32_t cluster) {
    return m_clusterHeapStartSector +
           ((cluster - 2) << m_sectorsPerClusterShift);
  }
  uint8_t* dirCache(DirPos_t* pos, uint8_t options);
  int8_t dirSeek(DirPos_t* pos, uint32_t offset);
  uint8_t fatGet(uint32_t cluster, uint32_t* value);
  bool fatPut(uint32_t cluster, uint32_t value);
  uint32_t chainSize(uint32_t cluster);
  bool freeChain(uint32_t cluster);
  uint16_t sectorMask() {return m_sectorMask;}
  bool syncDevice() {
    return m_blockDev->syncDevice();
  }
  bool readSector(uint32_t sector, uint8_t* dst) {
    return m_blockDev->readSector(sector, dst);
  }
  bool readSectors(uint32_t sector, uint8_t* dst, size_t count) {
    return m_blockDev->readSectors(sector, dst, count);
  }
  bool writeSector(uint32_t sector, const uint8_t* src) {
    return m_blockDev->writeSector(sector, src);
  }
  bool writeSectors(uint32_t sector, const uint8_t* src, size_t count) {
    return m_blockDev->writeSectors(sector, src, count);
  }
  //----------------------------------------------------------------------------
  static const uint8_t  m_bytesPerSectorShift = 9;
  static const uint16_t m_bytesPerSector = 512;
  static const uint16_t m_sectorMask = 0x1FF;
  //----------------------------------------------------------------------------
#if USE_EXFAT_BITMAP_CACHE
  FsCache  m_bitmapCache;
#endif  // USE_EXFAT_BITMAP_CACHE
  FsCache  m_dataCache;
  uint32_t m_bitmapStart;
  uint32_t m_fatStartSector;
  uint32_t m_fatLength;
  uint32_t m_clusterHeapStartSector;
  uint32_t m_clusterCount;
  uint32_t m_rootDirectoryCluster;
  uint32_t m_clusterMask;
  uint32_t m_bytesPerCluster;
  BlockDevice* m_blockDev;
  uint8_t  m_fatType;
  uint8_t  m_sectorsPerClusterShift;
};
#endif  // ExFatPartition_h
