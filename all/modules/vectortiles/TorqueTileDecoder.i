#ifndef _TORQUETILEDECODER_I
#define _TORQUETILEDECODER_I

%module TorqueTileDecoder

!proxy_imports(carto::TorqueTileDecoder, core.BinaryData, graphics.Color, styles.CartoCSSStyleSet, vectortiles.VectorTileDecoder)

%{
#include "vectortiles/TorqueTileDecoder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/BinaryData.i"
%import "styles/CartoCSSStyleSet.i"
%import "vectortiles/VectorTileDecoder.i"

!polymorphic_shared_ptr(carto::TorqueTileDecoder, vectortiles.TorqueTileDecoder)

%attribute(carto::TorqueTileDecoder, int, FrameCount, getFrameCount)
%attribute(carto::TorqueTileDecoder, int, Resolution, getResolution)
%attribute(carto::TorqueTileDecoder, float, AnimationDuration, getAnimationDuration)
%attributestring(carto::TorqueTileDecoder, std::shared_ptr<carto::CartoCSSStyleSet>, StyleSet, getStyleSet, setStyleSet)
%std_exceptions(carto::TorqueTileDecoder::TorqueTileDecoder)
%std_exceptions(carto::TorqueTileDecoder::setStyleSet)
%ignore carto::TorqueTileDecoder::decodeFeature;
%ignore carto::TorqueTileDecoder::decodeFeatures;
%ignore carto::TorqueTileDecoder::decodeTile;
%ignore carto::TorqueTileDecoder::getMapSettings;
%ignore carto::TorqueTileDecoder::getNutiParameters;

%include "vectortiles/TorqueTileDecoder.h"

#endif
