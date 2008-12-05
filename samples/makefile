all: .phony

.phony:
	cd BuildLightmaps && make
	cd BunnyBenchmark && make
	cd CPULightmaps && make
	cd HelloCollider && make
	cd HelloMesh && make
	cd Lightmaps && make
	cd MultiMeshCollider && make
	cd PenumbraShadows && make
	cd RealtimeLights && make
	cd RealtimeRadiosity && make
	cd SceneViewer && make

clean:
	cd BuildLightmaps && make clean
	cd BunnyBenchmark && make clean
	cd CPULightmaps && make clean
	cd HelloCollider && make clean
	cd HelloMesh && make clean
	cd Lightmaps && make clean
	cd MultiMeshCollider && make clean
	cd PenumbraShadows && make clean
	cd RealtimeLights && make clean
	cd RealtimeRadiosity && make clean
	cd SceneViewer && make clean

%:
	cd BuildLightmaps && make $@
	cd BunnyBenchmark && make $@
	cd CPULightmaps && make $@
	cd HelloCollider && make $@
	cd HelloMesh && make $@
	cd Lightmaps && make $@
	cd MultiMeshCollider && make $@
	cd PenumbraShadows && make $@
	cd RealtimeLights && make $@
	cd RealtimeRadiosity && make $@
	cd SceneViewer && make $@
