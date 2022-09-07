set(LIBRARIES "dpsim-villas")

if (NOT WIN32)
	# needed so CIMParser and arabica (which are first compiled into static libs)
	# can be included in the .so
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

list(APPEND LIBRARIES ${VILLASNODE_LIBRARIES})
list(APPEND LIBRARIES "jansson")
list(APPEND LIBRARIES "uuid")
list(APPEND LIBRARIES "villas-common")
list(APPEND INCLUDE_DIRS ${VILLASNODE_INCLUDE_DIRS})

set(SHMEM_SOURCES
	FileExample.cpp
	MqttExample.cpp
	SharedMemExample.cpp
	ShmemExample.cpp
	ShmemDistributedReference.cpp
	ShmemDistributedDirect.cpp
	ShmemDistributedVillas.cpp
	ShmemControllableSource.cpp
	ShmemControllableFiltSource.cpp
	Shmem_CIGRE_MV_PowerFlowTest.cpp
	Shmem_CIGRE_MV_PowerFlowTest_LoadProfiles.cpp
)

if(WITH_CIM)
	set(CIM_SHMEM_SOURCES
		Shmem_WSCC-9bus.cpp
		Shmem_WSCC-9bus_Ctrl.cpp
		Shmem_WSCC-9bus_CtrlDist.cpp
	)
endif()

foreach(SOURCE ${SHMEM_SOURCES} ${CIM_SHMEM_SOURCES})
	get_filename_component(TARGET ${SOURCE} NAME_WE)

	add_executable(${TARGET} ${SOURCE})

	target_link_libraries(${TARGET} ${LIBRARIES})
	target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})
	target_compile_options(${TARGET} PUBLIC ${DPSIM_CXX_FLAGS})
endforeach()

add_executable(ShmemRealtimeExample ShmemExample.cpp)
target_link_libraries(ShmemRealtimeExample ${LIBRARIES})
target_include_directories(ShmemRealtimeExample PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(ShmemRealtimeExample PUBLIC REALTIME)