#include "landscape.h"

#include <Tempest/Log>
#include <cstddef>
#include <chrono> // TEMP diag

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  auto& device = Resources::device();

  meshletDesc = Resources::ssbo(packed.meshletBounds.data(), packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));
  bvhNodes    = Resources::ssbo(packed.bvhNodes.data(),  packed.bvhNodes.size()*sizeof(packed.bvhNodes[0]));
  //bvhNodes    = Resources::ssbo(packed.bvh8Nodes.data(), packed.bvh8Nodes.size()*sizeof(packed.bvh8Nodes[0]));

  blocks.reserve(packed.subMeshes.size());
  Tempest::Log::i("[loadstage] TEMP Landscape: ", packed.subMeshes.size(), " submeshes");
  double matMs=0, visMs=0; size_t drawn=0, skipped=0; // TEMP diag
  for(size_t i=0; i<packed.subMeshes.size(); ++i) {
    auto& sub      = packed.subMeshes[i];
    auto  id       = uint32_t(sub.iboOffset/PackedMesh::MaxInd);
    auto  t0       = std::chrono::steady_clock::now(); // TEMP
    auto  material = Resources::loadMaterial(sub.material,true);
    auto  t1       = std::chrono::steady_clock::now(); // TEMP
    matMs += std::chrono::duration<double,std::milli>(t1-t0).count(); // TEMP

    if(material.alpha==Material::AdditiveLight || sub.iboLength==0) {
      ++skipped; // TEMP
      continue;
      }

    if(Gothic::options().doRayQuery) {
      mesh.sub[i].blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
      }

    Block b;
    auto  t2 = std::chrono::steady_clock::now(); // TEMP
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,&packed.meshletBounds[id],DrawCommands::Landscape);
    auto  t3 = std::chrono::steady_clock::now(); // TEMP
    visMs += std::chrono::duration<double,std::milli>(t3-t2).count(); ++drawn; // TEMP
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));

    if((i%256)==0) // TEMP
      Tempest::Log::i("[loadstage] TEMP Landscape i=",i," drawn=",drawn," matMs=",int(matMs)," visMs=",int(visMs)); // TEMP
    }
  Tempest::Log::i("[loadstage] TEMP Landscape DONE drawn=",drawn," skipped=",skipped," matMs=",int(matMs)," visMs=",int(visMs)); // TEMP
  }
