#pragma once
#include <list>
#include <map>
#include <mutex>
#include <vector>
#include <fstream>
#include <random>
#include <atomic>
#include "Logger.hpp"
#include "logger_instance.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "worlddb.hpp"
// #include "registry.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <netdb.h>
#endif

void backupLevel(const string& name, const string& path);
int getLatestBackup(const string& name);
/*
   struct BigChunkKey{
   int16_t bcx, bcz;
   bool operator<(const BigChunkKey& o) const {
   if(bcx != o.bcx) return bcx < o.bcx;
   return bcz < o.bcz;
   }
   bool operator==(const BigChunkKey& o1) const {
   if(o1.bcx == bcx)
   if(o1.bcz == bcz)
   return true;
   return false;
   }

   };

   struct BigChunkIndex{
   BigChunkKey key;
   uint64_t offset;
   };*/
struct Block {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t id;
};

class Level {
	public:
		int sizeX, sizeY, sizeZ;
		int64_t worldId;
		vector<Block> blocks;
		atomic<bool> dirty{false};

		Level(int x, int y, int z) : sizeX(x), sizeY(y), sizeZ(z){
			blocks.resize(x * y * z, 0x00);
		}

		Level(int32_t x, int32_t y, int32_t z, int64_t wid) 
			: sizeX(x), sizeY(y), sizeZ(z), worldId(wid) {}


		void setBlock(int x, int y, int z, uint8_t id){
			if(x < 0 || x>= sizeX || y < 0 || y >= sizeY || z < 0 || z >= sizeZ) return;
			blocks[x + z * sizeX + y * sizeX * sizeZ] = id;
			dirty = true;
		}

		uint8_t getBlock(int x, int y, int z){
			if(x < 0 || x >= sizeX || y < 0 || y >= sizeY || z < 0 || z >= sizeZ) return 0;
			return blocks[x+z*sizeX+y*sizeX*sizeZ];
		}

		void addBlock(int16_t x, int16_t y, int16_t z, uint8_t id) {
			blocks.push_back({x, y, z, id});
		}

	void newFile() {
        blocks.clear();
        int groundY = sizeY / 2;
        for (int iz = 0; iz < sizeZ; iz++) {
            for (int ix = 0; ix < sizeX; ix++) {
                addBlock(ix, groundY - 2, iz, 3);  // dirt
                addBlock(ix, groundY - 1, iz, 3);  // dirt
                addBlock(ix, groundY, iz, 2);      // grass
            }
        }
        logger.info("Generated new level");
    }

		void save(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            logger.err("Failed to open level file for writing: " + filename);
            return;
        }

        // Write each block as 7 bytes: x(2) + z(2) + y(2) + id(1)
        for (const auto& block : blocks) {
            uint8_t buf[7];
            buf[0] = (block.x >> 8) & 0xFF;
            buf[1] = block.x & 0xFF;
            buf[2] = (block.z >> 8) & 0xFF;
            buf[3] = block.z & 0xFF;
            buf[4] = (block.y >> 8) & 0xFF;
            buf[5] = block.y & 0xFF;
            buf[6] = block.id;
            file.write((char*)buf, 7);
        }

        file.close();
        logger.info("Level saved to " + filename);
    }

	void load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            logger.err("Failed to open level file: " + filename);
            return;
        }

        blocks.clear();
        uint8_t buf[7];
        while (file.read((char*)buf, 7)) {
            int16_t x = (int16_t)((buf[0] << 8) | buf[1]);
            int16_t z = (int16_t)((buf[2] << 8) | buf[3]);
            int16_t y = (int16_t)((buf[4] << 8) | buf[5]);
            uint8_t id = buf[6];
            blocks.push_back({x, y, z, id});
        }

        file.close();
        logger.info("Level loaded from " + filename + " (" + std::to_string(blocks.size()) + " blocks)");
    }
};
/*
   class InfiniteLevel {
   public:
   static const int BC_X = 256;
   static const int BC_Y = 64;
   static const int BC_Z = 256;
   static const int BC_BLOCKS = BC_X * BC_Y * BC_Z; // 4,194,304

   string filePath;
   uint64_t seed;
   mutex fileMutex;

   map<BigChunkKey, uint64_t> index;       // key -> file offset of block array
   map<BigChunkKey, Level*> cache;         // loaded big-chunks
   map<BigChunkKey, bool> dirty;           // which cached chunks need saving
   list<BigChunkKey> lruOrder;             // front = most recent
   static const int MAX_CACHE = 16;

   InfiniteLevel(const string& path, uint64_t worldSeed) : filePath(path), seed(worldSeed) {}

   ~InfiniteLevel(){
   flush();
   for(auto& pair : cache) delete pair.second;
   }

// --- File I/O helpers ---

void writeIndex(fstream& f){
// Index is always at byte 13 (after magic:8, mode:1, seed:8 = 17... wait:
// magic:8 + mode:1 + seed:8 = 17, then chunkCount:4 at byte 17)
f.seekp(17);
uint32_t count = (uint32_t)index.size();
uint8_t cb[4];
cb[0] = (count >> 24) & 0xFF;
cb[1] = (count >> 16) & 0xFF;
cb[2] = (count >>  8) & 0xFF;
cb[3] =  count        & 0xFF;
f.write((char*)cb, 4);

for(auto& pair : index){
int16_t bcx = pair.first.bcx;
int16_t bcz = pair.first.bcz;
uint64_t off = pair.second;
uint8_t entry[12];
entry[0]  = (bcx >> 8) & 0xFF; entry[1]  = bcx & 0xFF;
entry[2]  = (bcz >> 8) & 0xFF; entry[3]  = bcz & 0xFF;
entry[4]  = (off >> 56) & 0xFF; entry[5]  = (off >> 48) & 0xFF;
entry[6]  = (off >> 40) & 0xFF; entry[7]  = (off >> 32) & 0xFF;
entry[8]  = (off >> 24) & 0xFF; entry[9]  = (off >> 16) & 0xFF;
entry[10] = (off >>  8) & 0xFF; entry[11] =  off        & 0xFF;
f.write((char*)entry, 12);
}
}

void writeBigChunkBlocks(fstream& f, uint64_t offset, Level* lvl){
f.seekp(offset);
// same chunk order as finite save()
int chunksX = (BC_X + 15) / 16;
int chunksY = (BC_Y + 15) / 16;
int chunksZ = (BC_Z + 15) / 16;
vector<uint8_t> buf;
buf.reserve(BC_BLOCKS);
for(int cy = 0; cy < chunksY; cy++)
for(int cz = 0; cz < chunksZ; cz++)
for(int cx = 0; cx < chunksX; cx++)
for(int ly = 0; ly < 16; ly++)
for(int lz = 0; lz < 16; lz++)
for(int lx = 0; lx < 16; lx++){
int wx = cx*16+lx, wy = cy*16+ly, wz = cz*16+lz;
buf.push_back((wx<BC_X && wy<BC_Y && wz<BC_Z) ? lvl->getBlock(wx,wy,wz) : 0x00);
}
f.write((char*)buf.data(), buf.size());
}

void readBigChunkBlocks(fstream& f, uint64_t offset, Level* lvl){
	f.seekg(offset);
	int chunksX = (BC_X + 15) / 16;
	int chunksY = (BC_Y + 15) / 16;
	int chunksZ = (BC_Z + 15) / 16;
	vector<uint8_t> buf(BC_BLOCKS);
	f.read((char*)buf.data(), BC_BLOCKS);
	int i = 0;
	for(int cy = 0; cy < chunksY; cy++)
		for(int cz = 0; cz < chunksZ; cz++)
			for(int cx = 0; cx < chunksX; cx++)
				for(int ly = 0; ly < 16; ly++)
					for(int lz = 0; lz < 16; lz++)
						for(int lx = 0; lx < 16; lx++, i++){
							int wx = cx*16+lx, wy = cy*16+ly, wz = cz*16+lz;
							if(wx<BC_X && wy<BC_Y && wz<BC_Z)
								lvl->setBlock(wx, wy, wz, buf[i]);
						}
}

// --- Create a brand new infinite world file ---

static InfiniteLevel* createNew(const string& path, uint64_t worldSeed){
	fstream f(path, ios::out | ios::binary);
	if(!f){ logger.err("Failed to create infinite level: " + path); return nullptr; }

	f.write("CCRMCLVL", 8);
	uint8_t mode = 0xFF;
	f.write((char*)&mode, 1);

	uint8_t sb[8];
	for(int i = 0; i < 8; i++) sb[i] = (worldSeed >> (56 - i*8)) & 0xFF;
	f.write((char*)sb, 8);

	// chunk count = 0
	uint8_t zero4[4] = {0,0,0,0};
	f.write((char*)zero4, 4);
	// no index entries yet
	f.close();

	logger.info("Created infinite level: " + path);
	return new InfiniteLevel(path, worldSeed);
}

// --- Load existing infinite world file ---

static InfiniteLevel* loadExisting(const string& path){
	fstream f(path, ios::in | ios::binary);
	if(!f){ logger.err("Failed to open infinite level: " + path); return nullptr; }

	char magic[8];
	f.read(magic, 8);
	if(memcmp(magic, "CCRMCLVL", 8) != 0){
		logger.err("Bad magic in: " + path); return nullptr;
	}
	uint8_t mode;
	f.read((char*)&mode, 1);
	if(mode != 0xFF){
		logger.err("Not an infinite level: " + path); return nullptr;
	}

	uint8_t sb[8];
	f.read((char*)sb, 8);
	uint64_t worldSeed = 0;
	for(int i = 0; i < 8; i++) worldSeed = (worldSeed << 8) | sb[i];

	uint8_t cb[4];
	f.read((char*)cb, 4);
	uint32_t count = ((uint32_t)cb[0]<<24)|((uint32_t)cb[1]<<16)|((uint32_t)cb[2]<<8)|cb[3];

	InfiniteLevel* lvl = new InfiniteLevel(path, worldSeed);

	for(uint32_t i = 0; i < count; i++){
		uint8_t entry[12];
		f.read((char*)entry, 12);
		int16_t bcx = (int16_t)((entry[0] << 8) | entry[1]);
		int16_t bcz = (int16_t)((entry[2] << 8) | entry[3]);
		uint64_t off = 0;
		for(int j = 4; j < 12; j++) off = (off << 8) | entry[j];
		lvl->index[{bcx, bcz}] = off;
	}
	f.close();

	logger.info("Loaded infinite level: " + path + " (" + to_string(count) + " big-chunks)");
	return lvl;
}

// --- Generation (flat for now, Step 2 will add noise) ---

void generateBigChunk(Level* lvl){
	fill(lvl->blocks.begin(), lvl->blocks.end(), 0x00);
	for(int iz = 0; iz < BC_Z; iz++)
		for(int ix = 0; ix < BC_X; ix++){
			lvl->setBlock(ix, 0,  iz, 7); // bedrock
			for(int iy = 1; iy <= 27; iy++) lvl->setBlock(ix, iy, iz, 1); // stone
			lvl->setBlock(ix, 28, iz, 3); // dirt
			lvl->setBlock(ix, 29, iz, 3); // dirt
			lvl->setBlock(ix, 30, iz, 3); // dirt
			lvl->setBlock(ix, 31, iz, 2); // grass
		}
	lvl->dirty = false;
}

// --- Get or generate a big-chunk (main entry point) ---

Level* getBigChunk(int16_t bcx, int16_t bcz){
	lock_guard<mutex> lock(fileMutex);
	BigChunkKey key{bcx, bcz};

	// Check cache
	auto cit = cache.find(key);
	if(cit != cache.end()){
		// Move to front of LRU
		lruOrder.remove(key);
		lruOrder.push_front(key);
		return cit->second;
	}

	// Evict if cache full
	if((int)cache.size() >= MAX_CACHE){
		BigChunkKey evict = lruOrder.back();
		lruOrder.pop_back();
		if(dirty[evict]){
			fstream f(filePath, ios::in | ios::out | ios::binary);
			writeBigChunkBlocks(f, index[evict], cache[evict]);
			dirty[evict] = false;
		}
		delete cache[evict];
		cache.erase(evict);
		dirty.erase(evict);
	}

	Level* lvl = new Level(BC_X, BC_Y, BC_Z);

	auto iit = index.find(key);
	if(iit != index.end()){
		// Load from file
		fstream f(filePath, ios::in | ios::binary);
		readBigChunkBlocks(f, iit->second, lvl);
		logger.info("Loaded big-chunk (" + to_string(bcx) + "," + to_string(bcz) + ")");
	} else {
		// Generate and append to file
		generateBigChunk(lvl);

		fstream f(filePath, ios::in | ios::out | ios::binary);
		f.seekp(0, ios::end);
		uint64_t offset = (uint64_t)f.tellp();
		writeBigChunkBlocks(f, offset, lvl);
		index[key] = offset;
		writeIndex(f);
		f.close();

		logger.info("Generated big-chunk (" + to_string(bcx) + "," + to_string(bcz) + ")");
	}

	cache[key] = lvl;
	dirty[key] = false;
	lruOrder.push_front(key);
	return lvl;
}

void markDirty(int16_t bcx, int16_t bcz){
	lock_guard<mutex> lock(fileMutex);
	BigChunkKey key{bcx, bcz};
	if(cache.count(key)) dirty[key] = true;
}

void flush(){
	lock_guard<mutex> lock(fileMutex);
	fstream f(filePath, ios::in | ios::out | ios::binary);
	if(!f){ logger.err("flush: cannot open " + filePath); return; }
	for(auto& pair : dirty){
		if(!pair.second) continue;
		auto cit = cache.find(pair.first);
		if(cit == cache.end()) continue;
		writeBigChunkBlocks(f, index[pair.first], cit->second);
		pair.second = false;
	}
	writeIndex(f);
	logger.info("Flushed infinite level: " + filePath);
}
};*/

class LevelRegistry {
public:
    std::mutex registryMutex;
    std::map<std::string, Level*> levels;

    Level* getOrLoad(const std::string& name, bool generate = false) {
        lock_guard<std::mutex> lock(registryMutex);
        auto it = levels.find(name);
        if (it != levels.end()) return it->second;

        // Get world record
        WorldRecord worldRec;
        if (!worldDB.getByName(name, worldRec)) {
            if (!generate) return nullptr;
            // Create new world
            worldRec.rowid = worldDB.createWorld(name, 256, 64, 256);
            if (worldRec.rowid < 0) return nullptr;
            worldRec.name = name;
            worldRec.sizeX = 256;
            worldRec.sizeY = 64;
            worldRec.sizeZ = 256;
        }

        Level* lvl = new Level(worldRec.sizeX, worldRec.sizeY, worldRec.sizeZ, worldRec.rowid);

        std::string path = "maps/" + name + ".lvl";
        std::ifstream check(path);
        if (check.good()) {
            check.close();
            lvl->load(path);
        } else if (generate) {
            lvl->newFile();
            lvl->save(path);
        } else {
            delete lvl;
            return nullptr;
        }

        levels[name] = lvl;
        logger.info("Loaded level: " + name);
        return lvl;
    }

    void unloadIfEmpty(const std::string& name) {
        if (name == "main") return;
        lock_guard<std::mutex> lock(registryMutex);
        auto it = levels.find(name);
        if (it == levels.end()) return;
        it->second->save("maps/" + name + ".lvl");
        delete it->second;
        levels.erase(it);
        logger.info("Unloaded level: " + name);
    }

    void saveAll() {
        lock_guard<std::mutex> lock(registryMutex);
        for (auto& pair : levels)
            pair.second->save("maps/" + pair.first + ".lvl");
    }
};

// util
vector<string> listLevelFiles();

void switchWorld(Player* player, const string& targetName);
