#include <zlib.h>
#include "packet.hpp"
#include "level.hpp"

void Packet::sendLevel(SOCKET socket, Level& level){
    int x = level.sizeX, y = level.sizeY, z = level.sizeZ;
    int totalBlocks = x * y * z;
    vector<uint8_t> levelData(4 + totalBlocks);

    levelData[0] = (totalBlocks >> 24) & 0xFF;
    levelData[1] = (totalBlocks >> 16) & 0xFF;
    levelData[2] = (totalBlocks >> 8) & 0xFF;
    levelData[3] = totalBlocks & 0xFF;

    memcpy(levelData.data() + 4, level.blocks.data(), totalBlocks);

    // COMPRESS
    uLongf compressedSize = compressBound(levelData.size()) + 18;
    vector<uint8_t> compressed(compressedSize);

    z_stream zs = {};
    int ret = deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK){ logger.err("Deflating failed: " + to_string(ret)); return; }

    zs.next_in = levelData.data();
    zs.avail_in = (uInt)levelData.size();
    zs.next_out = compressed.data();
    zs.avail_out = (uInt)compressedSize;

    ret = deflate(&zs, Z_FINISH);
    if(ret != Z_STREAM_END){
        logger.err("Deflating did not finish: " + to_string(ret));
        deflateEnd(&zs);
        return;
    }
    compressedSize = zs.total_out;
    deflateEnd(&zs);
    compressed.resize(compressedSize);
    logger.debug("Compr. size: " + to_string(compressedSize));
    logger.debug("Chunks to send: " + to_string((compressedSize + 1023) / 1024));

    // SEND PACKETS
    uint8_t initPacket = 0x02;
    send(socket, (char*)&initPacket, 1, 0);

    size_t offset = 0;
    size_t totalSize = compressed.size();

    while(offset < totalSize){
        char chunkPacket[1028] = {};
        size_t chunkLen = min((size_t)1024, totalSize - offset);
        uint8_t percent = (uint8_t)((offset + chunkLen) * 100 / totalSize);

        chunkPacket[0] = 0x03;
        chunkPacket[1] = (chunkLen >> 8) & 0xFF;
        chunkPacket[2] = chunkLen & 0xFF;
        memcpy(chunkPacket + 3, compressed.data() + offset, chunkLen);
        chunkPacket[1027] = (char)percent;

        send(socket, chunkPacket, 1028, 0);
        offset += chunkLen;
    }

    uint8_t finalPacket[7];
    uint16_t sx = (uint16_t)x;
    uint16_t sy = (uint16_t)y;
    uint16_t sz = (uint16_t)z;

    finalPacket[0] = 0x04;
    finalPacket[1] = (sx >> 8) & 0xFF; finalPacket[2] = sx & 0xFF;
    finalPacket[3] = (sy >> 8) & 0xFF; finalPacket[4] = sy & 0xFF;
    finalPacket[5] = (sz >> 8) & 0xFF; finalPacket[6] = sz & 0xFF;
    send(socket, (char*)finalPacket, sizeof(finalPacket), 0);
}
