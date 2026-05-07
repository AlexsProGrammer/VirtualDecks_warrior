# src/core/analysis/CMakeLists.txt

set(ANALYSIS_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/BpmDetector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/BpmAnalysisManager.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/BeatSyncManager.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/WaveformBandAnalyzer.cpp"
	PARENT_SCOPE
)
