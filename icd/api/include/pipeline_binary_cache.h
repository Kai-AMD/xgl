/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
***********************************************************************************************************************
* @file  pipeline_binary_cache.h
* @brief Declaration of Vulkan interface for a PAL layered cache specializing in pipeline binaries
***********************************************************************************************************************
*/

#include "pipeline_compiler.h"

#include "palHashMap.h"
#include "palMetroHash.h"
#include "palVector.h"
#include "palCacheLayer.h"

namespace Util
{
class IPlatformKey;
} // namespace Util

namespace vk
{

struct BinaryCacheEntry
{
    Util::MetroHash::Hash hashId;
    size_t                dataSize;
};

constexpr size_t SHA_DIGEST_LENGTH = 20;
struct PipelineBinaryCachePrivateHeader
{
    uint8_t  hashId[SHA_DIGEST_LENGTH];
};

// Unified pipeline cache interface
class PipelineBinaryCache
{
public:
    using CacheId                    = Util::MetroHash::Hash;

    static PipelineBinaryCache* Create(
        Instance*                 pInstance,
        size_t                    initDataSize,
        const void*               pInitData,
        bool                      internal,
        const Llpc::GfxIpVersion& gfxIp,
        const PhysicalDevice*     pPhysicalDevice);

    static bool IsValidBlob(
        const PhysicalDevice* pPhysicalDevice,
        size_t dataSize,
        const void* pData);

    VkResult Initialize(
        const PhysicalDevice* pPhysicalDevice);

    Util::Result QueryPipelineBinary(
        const CacheId*     pCacheId,
        Util::QueryResult* pQuery);

    Util::Result LoadPipelineBinary(
        const CacheId*  pCacheId,
        size_t*         pPipelineBinarySize,
        const void**    ppPipelineBinary) const;

    Util::Result StorePipelineBinary(
        const CacheId*  pCacheId,
        size_t          pipelineBinarySize,
        const void*     pPipelineBinary);

    VkResult Serialize(
        void*   pBlob,
        size_t* pSize);

    VkResult Merge(
        uint32_t                    srcCacheCount,
        const PipelineBinaryCache** ppSrcCaches);

#if ICD_GPUOPEN_DEVMODE_BUILD
    Util::Result LoadReinjectionBinary(
        const CacheId*           pInternalPipelineHash,
        size_t*                  pPipelineBinarySize,
        const void**             ppPipelineBinary);

    Util::Result StoreReinjectionBinary(
        const CacheId*           pInternalPipelineHash,
        size_t                   pipelineBinarySize,
        const void*              pPipelineBinary);

    using HashMapping = Util::HashMap<Pal::PipelineHash, CacheId, PalAllocator>;

    void RegisterHashMapping(
        const Pal::PipelineHash* pInternalPipelineHash,
        const CacheId*           pCacheId);

    CacheId* GetCacheIdForPipeline(
        const Pal::PipelineHash* pInternalPipelineHash);

    VK_INLINE HashMapping::Iterator GetHashMappingIterator()
        { return m_hashMapping.Begin(); }

    VK_INLINE Util::RWLock* GetHashMappingLock()
        { return &m_hashMappingLock; }
#endif

    void FreePipelineBinary(const void* pPipelineBinary);

    void Destroy() { this->~PipelineBinaryCache(); }

private:

    PAL_DISALLOW_DEFAULT_CTOR(PipelineBinaryCache);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineBinaryCache);

    explicit PipelineBinaryCache(
        Instance*                 pInstance,
        const Llpc::GfxIpVersion& gfxIp,
        bool                      internal);
    ~PipelineBinaryCache();

    VkResult InitializePlatformKey(
        const PhysicalDevice*  pPhysicalDevice,
        const RuntimeSettings& settings);

    VkResult OrderLayers(
        const RuntimeSettings& settings);

    VkResult AddLayerToChain(
        Util::ICacheLayer*  pLayer,
        Util::ICacheLayer** pBottomLayer);

    VkResult InitLayers(
        const PhysicalDevice*  pPhysicalDevice,
        bool                   internal,
        const RuntimeSettings& settings);

#if ICD_GPUOPEN_DEVMODE_BUILD
    VkResult InitReinjectionLayer(
        const RuntimeSettings& settings);

    Util::Result InjectBinariesFromDirectory(
        const RuntimeSettings& settings);
#endif

    VkResult InitMemoryCacheLayer(
        const RuntimeSettings& settings);

    VkResult InitArchiveLayers(
        const PhysicalDevice*  pPhysicalDevice,
        const RuntimeSettings& settings);

    Util::ICacheLayer*  GetMemoryLayer() const { return m_pMemoryLayer; }
    Util::IArchiveFile* OpenReadOnlyArchive(const char* path, const char* fileName, size_t bufferSize);
    Util::IArchiveFile* OpenWritableArchive(const char* path, const char* fileName, size_t bufferSize);
    Util::ICacheLayer*  CreateFileLayer(Util::IArchiveFile* pFile);

    // Override the driver's default location
    static constexpr char   EnvVarPath[] = "AMD_VK_PIPELINE_CACHE_PATH";

    // Override the driver's default name (Hash of application name)
    static constexpr char   EnvVarFileName[] = "AMD_VK_PIPELINE_CACHE_FILENAME";

    // Filename of an additional, read-only archive
    static constexpr char   EnvVarReadOnlyFileName[] = "AMD_VK_PIPELINE_CACHE_READ_ONLY_FILENAME";

    static const uint32_t   ArchiveType;                // TypeId created by hashed string VK_SHADER_PIPELINE_CACHE
    static const uint32_t   ElfType;                    // TypeId created by hashed string VK_PIPELINE_ELF

    Llpc::GfxIpVersion      m_gfxIp;                    // Compared against e_flags of reinjected elf files

    Instance* const         m_pInstance;                // Allocator for use when interacting with the cache

    const Util::IPlatformKey*     m_pPlatformKey;       // Platform identifying key

    Util::ICacheLayer*      m_pTopLayer;                // Top layer of the cache chain where queries are submitted

#if ICD_GPUOPEN_DEVMODE_BUILD
    Util::ICacheLayer*      m_pReinjectionLayer;        // Reinjection interface layer

    HashMapping             m_hashMapping;              // Maps the internalPipelineHash to the appropriate CacheId
    Util::RWLock            m_hashMappingLock;          // Prevents collisions during writes to the map
#endif

    Util::ICacheLayer*      m_pMemoryLayer;

    // Archive based cache layers
    using FileVector  = Util::Vector<Util::IArchiveFile*, 8, PalAllocator>;
    using LayerVector = Util::Vector<Util::ICacheLayer*, 8, PalAllocator>;
    Util::ICacheLayer*  m_pArchiveLayer;  // Top of a chain of loaded archives.
    FileVector          m_openFiles;
    LayerVector         m_archiveLayers;

    bool                m_isInternalCache;
};

} // namespace vk
