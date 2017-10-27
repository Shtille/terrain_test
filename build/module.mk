# This file provides the following to the build system:
#   MODULE_NAME
#   SRC_FILES
# it requires the following are provided
#   JUPITER_ROOT
#   COSMOS_ROOT
#   ROUTINGENG_ROOT
#   THIRDPARTY_ROOT

MODULE_NAME = mgnTerrain

################# Project Include Directories #################
# The preceeding "-I" will be removed for Android builds

LOCAL_C_INCLUDES   :=  \
  -I$(COSMOS_ROOT)/API/Include                                            \
  -I$(JUPITER_ROOT)/Nova/Utilities/mgnTerrain/include                     \
  -I$(JUPITER_ROOT)/Nova/Utilities/Include                                \
  -I$(JUPITER_ROOT)/Nova/Utilities/Core/include                           \
  -I$(JUPITER_ROOT)/Nova/Utilities/$(MODULE_NAME)/include                 \
  -I$(JUPITER_ROOT)/Nova/Utilities/MapDrawing/include                     \
  -I$(JUPITER_ROOT)/Nova/Utilities/MapDrawing/include/MapDrawing/Graphics                     \
  -I$(JUPITER_ROOT)/Nova/Utilities/NovaCosmosInterface/include            \
  -I$(JUPITER_ROOT)/Nova/Utilities/UGDS/include                                         \
  -I$(THIRDPARTY_ROOT)/libexpat                                                                        \
  -I$(THIRDPARTY_ROOT)/freetype/include                                                                \
  -I$(COSMOS_ROOT)/API/Include                                                                         \
  -I$(JUPITER_ROOT)/Nova/Utilities/RoadHighlight/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/ActiveRouteTraffic/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/RoutePlanner/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/Guidance/include \
  -I$(ROUTINGENG_ROOT)/API/Include \
  -I$(JUPITER_ROOT)/Nova/Utilities/NovaCosmosInterface/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/MapMatching/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/LdpUtils/include \
  -I$(THIRDPARTY_ROOT)/boost \
  -I$(JUPITER_ROOT)/Nova/Utilities/OnlineRasterMaps/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/TrailDB/include \
  -I$(JUPITER_ROOT)/Nova/Utilities/ExternalRteDB/include \
  
    #

################# Project Sources Directories #################

SRC_DIRS = \
  ./Nova/Utilities/$(MODULE_NAME)/src              \
  #

################# Additional Selected Sources Files #################

SPECIAL_SRC_FILES = \
  #

#################### Consolidate Sources ######################

dirs := $(SRC_DIRS)
T_C_SRC_FILES := $(foreach dir,$(dirs),$(wildcard $(JUPITER_ROOT)/$(dir)/*.c))
T_CPP_SRC_FILES := $(foreach dir,$(dirs),$(wildcard $(JUPITER_ROOT)/$(dir)/*.cpp))

SRC_FILES := $(T_CPP_SRC_FILES) $(T_C_SRC_FILES) $(subst ./,$(JUPITER_ROOT)/,$(SPECIAL_SRC_FILES))

LOCAL_STATIC_LIBRARIES := OnlineRasterMaps TrailDB
