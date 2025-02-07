#include "TorqueTileLayer.h"
#include "layers/VectorTileEventListener.h"
#include "renderers/MapRenderer.h"
#include "renderers/TileRenderer.h"
#include "vectortiles/TorqueTileDecoder.h"

#include <vt/Tile.h>

namespace carto {

    TorqueTileLayer::TorqueTileLayer(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<TorqueTileDecoder>& decoder) :
        VectorTileLayer(dataSource, decoder)
    {
        setTileMapsMode(true); // Torque uses tile maps
    }
    
    TorqueTileLayer::~TorqueTileLayer() {
    }

    int TorqueTileLayer::countVisibleFeatures(int frameNr) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        std::size_t count = 0;
        for (long long tileId : getVisibleTileIds()) {
            if (std::shared_ptr<VectorTileDecoder::TileMap> tileMap = getTileMap(tileId)) {
                auto it = tileMap->find(frameNr);
                if (it != tileMap->end()) {
                    count += it->second->getFeatureCount();
                }
            }
        }
        return static_cast<int>(count);
    }

    bool TorqueTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState) {
        updateTileLoadListener();

        if (auto mapRenderer = getMapRenderer()) {
            float opacity = getOpacity();

            Color backgroundColor = Color(getTileDecoder()->getMapSettings()->backgroundColor.value());
            mapRenderer->clearAndBindScreenFBO(backgroundColor, false, false);

            _tileRenderer->setLayerBlendingSpeed(0.0f);
            bool refresh = _tileRenderer->onDrawFrame(deltaSeconds, viewState);

            mapRenderer->blendAndUnbindScreenFBO(opacity);

            return refresh;
        }
        return false;
    }
        
}
