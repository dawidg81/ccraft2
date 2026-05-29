#pragma once
#include <vector>
#include <fstream>
#include <cstring>
#include <atomic>
#include "Logger.hpp"
#include "logger_instance.hpp"
#include "worlddb.hpp"

void backupLevel(const string& name, const string& path);

class Level {
public:
    int32_t sizeX, sizeY, sizeZ;
    int64_t worldId;  // world db
    std::vector<uint8_t> blocks;
    std::atomic<bool> dirty{false};

    Level(int32_t x, int32_t y, int32_t z, int64_t wid) 
        : sizeX(x), sizeY(y), sizeZ(z), worldId(wid) {
        blocks.resize((size_t)x * y * z, 0x00);
    }

    void setBlock(int x, int y, int z, uint8_t id) {
        if (x < 0 || x >= sizeX || y < 0 || y >= sizeY || z < 0 || z >= sizeZ) return;
        blocks[(size_t)x + (size_t)z * sizeX + (size_t)y * sizeX * sizeZ] = id;
        dirty = true;
    }

    uint8_t getBlock(int x, int y, int z) {
        if (x < 0 || x >= sizeX || y < 0 || y >= sizeY || z < 0 || z >= sizeZ) return 0;
        return blocks[(size_t)x + (size_t)z * sizeX + (size_t)y * sizeX * sizeZ];
    }

    void newFile() {
        std::fill(blocks.begin(), blocks.end(), 0x00);
        int groundY = sizeY / 2;
        for (int iz = 0; iz < sizeZ; iz++) {
            for (int ix = 0; ix < sizeX; ix++) {
                setBlock(ix, groundY - 2, iz, 3);  // dirt
                setBlock(ix, groundY - 1, iz, 3);  // dirt
                setBlock(ix, groundY, iz, 2);      // grass
            }
        }
        dirty = false;
        logger.info("Generated new level");
    }

    void save(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            logger.err("Failed to open level file for writing: " + filename);
            return;
        }

        for (int y = 0; y < sizeY; y++) {
            for (int z = 0; z < sizeZ; z++) {
                for (int x = 0; x < sizeX; x++) {
                    uint8_t id = getBlock(x, y, z);
                    if (id != 0x00) {
                        uint8_t buf[7];
                        buf[0] = (x >> 8) & 0xFF;
                        buf[1] = x & 0xFF;
                        buf[2] = (z >> 8) & 0xFF;
                        buf[3] = z & 0xFF;
                        buf[4] = (y >> 8) & 0xFF;
                        buf[5] = y & 0xFF;
                        buf[6] = id;
                        file.write((char*)buf, 7);
                    }
                }
            }
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

        std::fill(blocks.begin(), blocks.end(), 0x00);

        uint8_t buf[7];
        int blockCount = 0;
        while (file.read((char*)buf, 7)) {
            int16_t x = (int16_t)((buf[0] << 8) | buf[1]);
            int16_t z = (int16_t)((buf[2] << 8) | buf[3]);
            int16_t y = (int16_t)((buf[4] << 8) | buf[5]);
            uint8_t id = buf[6];
            setBlock(x, y, z, id);
            blockCount++;
        }

        file.close();
        logger.info("Level loaded from " + filename + " (" + std::to_string(blockCount) + " blocks)");
    }
};

class LevelRegistry {
public:
    std::mutex registryMutex;
    std::map<std::string, Level*> levels;

    Level* getOrLoad(const std::string& name, bool generate = false) {
        lock_guard<std::mutex> lock(registryMutex);
        auto it = levels.find(name);
        if (it != levels.end()) return it->second;

        WorldRecord worldRec;
        if (!worldDB.getByName(name, worldRec)) {
            if (!generate) return nullptr;

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