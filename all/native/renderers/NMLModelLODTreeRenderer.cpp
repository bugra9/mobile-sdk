#ifdef _CARTO_NMLMODELLODTREE_SUPPORT

#include "NMLModelLODTreeRenderer.h"
#include "datasources/NMLModelLODTreeDataSource.h"
#include "graphics/Shader.h"
#include "graphics/ShaderManager.h"
#include "graphics/ViewState.h"
#include "layers/NMLModelLODTreeLayer.h"
#include "projections/Projection.h"
#include "renderers/components/RayIntersectedElement.h"
#include "nml/Model.h"
#include "nml/ShaderManager.h"
#include "utils/Log.h"
#include "utils/GLES2.h"
#include "utils/GeomUtils.h"
#include "utils/GLUtils.h"

namespace carto {

    NMLModelLODTreeRenderer::NMLModelLODTreeRenderer() :
        _glShaderManager(),
        _tempDrawDatas(),
        _drawRecordMap(),
        _options(),
        _mutex()
    {
    }
    
    NMLModelLODTreeRenderer::~NMLModelLODTreeRenderer() {
    }
    
    void NMLModelLODTreeRenderer::addDrawData(const std::shared_ptr<NMLModelLODTreeDrawData>& drawData) {
        _tempDrawDatas.push_back(drawData);
    }
    
    void NMLModelLODTreeRenderer::refreshDrawData() {
        std::lock_guard<std::mutex> lock(_mutex);
    
        // Mark all existing records as unused, unlink node hierarchy
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            record.used = false;
            record.parent = 0;
            record.children.clear();
        }
    
        // Create new nodes, mark used nodes
        for (auto it = _tempDrawDatas.begin(); it != _tempDrawDatas.end(); it++) {
            std::shared_ptr<ModelNodeDrawRecord>& record = _drawRecordMap[(*it)->getNodeId()];
            if (!record) {
                record.reset(new ModelNodeDrawRecord(**it));
            } else {
                record->drawData = **it;
            }
            record->used = true;
        }
    
        // Build node hierarchy, create parent-child links between draw records
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            for (size_t i = 0; i < record.drawData.getParentIds().size(); i++) {
                auto it2 = _drawRecordMap.find(record.drawData.getParentIds()[i]);
                if (it2 != _drawRecordMap.end()) {
                    ModelNodeDrawRecord& parentRecord = *it2->second;
                    record.parent = &parentRecord;
                    parentRecord.children.push_back(&record);
                    break;
                }
            }
        }
    
        _tempDrawDatas.clear();
    }
    
    void NMLModelLODTreeRenderer::setOptions(const std::weak_ptr<Options>& options) {
        std::lock_guard<std::mutex> lock(_mutex);
        _options = options;
    }

    void NMLModelLODTreeRenderer::offsetLayerHorizontally(double offset) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            record.drawData.offsetHorizontally(offset);
        }
    }

    void NMLModelLODTreeRenderer::onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager) {
        _glShaderManager = std::make_shared<nmlgl::ShaderManager>();
        _drawRecordMap.clear();
    }

    bool NMLModelLODTreeRenderer::onDrawFrame(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);
    
        std::shared_ptr<Options> options = _options.lock();
        if (!options) {
            return false;
        }

        // Set expected GL state
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        // Calculate lighting state
        Color ambientColor = options->getAmbientLightColor();
        cglib::vec4<float> ambientLightColor = cglib::vec4<float>(ambientColor.getR(), ambientColor.getG(), ambientColor.getB(), ambientColor.getA()) * (1.0f / 255.0f);
        Color mainColor = options->getMainLightColor();
        cglib::vec4<float> mainLightColor = cglib::vec4<float>(mainColor.getR(), mainColor.getG(), mainColor.getB(), mainColor.getA()) * (1.0f / 255.0f);
        MapVec mainDir = options->getMainLightDirection();
        cglib::vec3<float> mainLightDir(mainDir.getX(), mainDir.getY(), mainDir.getZ());

        cglib::mat4x4<float> projMat = cglib::mat4x4<float>::convert(viewState.getProjectionMat());

        // Create new models
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            if (!(record.used && !record.created)) {
                continue;
            }
    
            record.drawData.getGLModel()->create(*_glShaderManager);
            record.created = true;
            break;
        }
    
        // If a model is used but not created, try to find its first parent that is created and mark it as used.  
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            if (!(record.used && !record.created)) {
                continue;
            }
    
            for (ModelNodeDrawRecord * parentRecord = record.parent; parentRecord; parentRecord = parentRecord->parent) {
                if (parentRecord->created) {
                    parentRecord->used = true;
                    break;
                }
            }
        }
    
        // If a model is not used but created then keep it if it has a parent that is used but not created. Also check that it does not have a closer parent that is created.  
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            if (!(!record.used && record.created)) {
                continue;
            }
    
            for (ModelNodeDrawRecord * parentRecord = record.parent; parentRecord; parentRecord = parentRecord->parent) {
                if (parentRecord->created) {
                    break;
                }
                if (parentRecord->used) {
                    record.used = true;
                    break;
                }
            }
        }
    
        // Draw nodes if they do not have parents that are also used/created
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            if (!record.used || !record.created) {
                continue;
            }
    
            bool draw = true;
            for (ModelNodeDrawRecord * parentRecord = record.parent; parentRecord; parentRecord = parentRecord->parent) {
                if (parentRecord->used && parentRecord->created) {
                    draw = false;
                    break;
                }
            }
            if (!draw) {
                continue;
            }

            cglib::mat4x4<float> mvMat = cglib::mat4x4<float>::convert(viewState.getModelviewMat() * record.drawData.getLocalMat());
            nmlgl::RenderState renderState(projMat, mvMat, ambientLightColor, mainLightColor, mainLightDir);

            record.drawData.getGLModel()->draw(renderState);
        }
    
        // Dispose all unused models, update parent-child links
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); ) {
            ModelNodeDrawRecord& record = *it->second;
            if (record.used) {
                it++;
                continue;
            }
    
            if (record.parent) {
                auto childIt = std::find(record.parent->children.begin(), record.parent->children.end(), &record);
                record.parent->children.erase(childIt);
                record.parent->children.insert(record.parent->children.end(), record.children.begin(), record.children.end());
            }
            for (size_t i = 0; i < record.children.size(); i++) {
                record.children[i]->parent = record.parent;
            }
            if (record.created) {
                record.drawData.getGLModel()->dispose();
            }
    
            _drawRecordMap.erase(it++);
        }
    
        // Check if we need to still update some models
        bool refresh = false;
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            ModelNodeDrawRecord& record = *it->second;
            if (!(record.used && !record.created)) {
                continue;
            }
    
            refresh = true;
        }
        
        // Restore expected GL state
        glDepthMask(GL_TRUE);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);

        return refresh;
    }

    void NMLModelLODTreeRenderer::onSurfaceDestroyed() {
        _glShaderManager.reset();
        _drawRecordMap.clear();
    }

    void NMLModelLODTreeRenderer::calculateRayIntersectedElements(const std::shared_ptr<NMLModelLODTreeLayer>& layer, const MapPos& rayOrig, const MapVec& rayDir, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);
    
        for (auto it = _drawRecordMap.begin(); it != _drawRecordMap.end(); it++) {
            const ModelNodeDrawRecord& record = *it->second;
            if (!(record.used && record.created)) {
                continue;
            }
    
            std::shared_ptr<nmlgl::Model> glModel = record.drawData.getGLModel();
            const cglib::mat4x4<double>& modelMat = record.drawData.getLocalMat();
            cglib::mat4x4<double> invModelMat = cglib::inverse(modelMat);
    
            cglib::vec3<double> rayOrigModel = cglib::transform_point(cglib::vec3<double>(rayOrig.getX(), rayOrig.getY(), rayOrig.getZ()), invModelMat);
            cglib::vec3<double> rayDirModel = cglib::transform_point(cglib::vec3<double>(rayOrig.getX() + rayDir.getX(), rayOrig.getY() + rayDir.getY(), rayOrig.getZ() + rayDir.getZ()), invModelMat) - rayOrigModel;
            cglib::bbox3<float> modelBounds = glModel->getBounds();
            
            if (!GeomUtils::RayBoundingBoxIntersect(
                    MapPos(rayOrigModel(0), rayOrigModel(1), rayOrigModel(2)),
                    MapVec(rayDirModel(0), rayDirModel(1), rayDirModel(2)),
                    MapBounds(MapPos(modelBounds.min(0), modelBounds.min(1), modelBounds.min(2)), MapPos(modelBounds.max(0), modelBounds.max(1), modelBounds.max(2)))))
            {
                continue;
            }
            
            std::vector<nmlgl::RayIntersection> intersections;
            glModel->calculateRayIntersections(nmlgl::Ray(rayOrigModel, rayDirModel), intersections);
            
            for (size_t i = 0; i < intersections.size(); i++) {
                NMLModelLODTree::ProxyMap::const_iterator proxy_it = record.drawData.getProxyMap()->find(intersections[i].vertexId);
                if (proxy_it == record.drawData.getProxyMap()->end()) {
                    continue;
                }
                
                cglib::vec3<double> pos = cglib::transform_point(intersections[i].pos, modelMat);
                MapPos clickPos(pos(0), pos(1), pos(2));
                double distance = GeomUtils::DistanceFromPoint(clickPos, viewState.getCameraPos());
                MapPos projectedClickPos = layer->getDataSource()->getProjection()->fromInternal(clickPos);
                int priority = static_cast<int>(results.size());
                results.push_back(RayIntersectedElement(std::static_pointer_cast<VectorElement>(proxy_it->second), layer, projectedClickPos, projectedClickPos, priority));
            }
        }
    }
    
}

#endif
