#include "track.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>

namespace flydemo {
namespace {
class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}
    bool U8(std::uint8_t& v) { if (p_>=b_.size()) return false; v=b_[p_++]; return true; }
    bool U16(std::uint16_t& v) {
        if (b_.size()-p_<2) return false;
        v=static_cast<std::uint16_t>(b_[p_]) | (static_cast<std::uint16_t>(b_[p_+1])<<8); p_+=2; return true;
    }
    bool F32(float& v) {
        if (b_.size()-p_<4) return false;
        std::uint32_t x=static_cast<std::uint32_t>(b_[p_]) |
            (static_cast<std::uint32_t>(b_[p_+1])<<8) |
            (static_cast<std::uint32_t>(b_[p_+2])<<16) |
            (static_cast<std::uint32_t>(b_[p_+3])<<24);
        std::memcpy(&v,&x,4); p_+=4; return true;
    }
    bool Tag5(const char* s) {
        if (b_.size()-p_<5) return false;
        for (int i=0;i<5;++i) if (b_[p_+i]!=static_cast<std::uint8_t>(s[i])) return false;
        p_+=5; return true;
    }
    bool CString256(std::string& s) {
        s.clear();
        for (std::size_t i=0;i<256;++i) {
            std::uint8_t c=0; if(!U8(c)) return false;
            if(c==0) return true;
            s.push_back(static_cast<char>(c));
        }
        return false;
    }
    std::size_t Pos() const { return p_; }
private:
    const std::vector<std::uint8_t>& b_; std::size_t p_=0;
};

bool Fail(std::string* e,const char* m){ if(e)*e=m; return false; }

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void AppendF32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "binary32 writer");
    std::memcpy(&bits, &value, sizeof(bits));
    out.push_back(static_cast<std::uint8_t>(bits & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xffu));
}

void AppendCString(std::vector<std::uint8_t>& out, const std::string& value) {

    out.insert(out.end(), value.begin(), value.end());
    out.push_back(0);
}
}

std::string BuildTrackPath(std::string_view requestedFilename) {

    return std::string("TRKDATA/TRACKS/") + std::string(requestedFilename);
}

std::string DeriveTrackLoadName(std::string_view requestedFilename) {

    if (requestedFilename.size() < 4)
        return {};
    std::string name(requestedFilename.substr(0, requestedFilename.size() - 4));

    for (char& ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= static_cast<unsigned char>('a') && c <= static_cast<unsigned char>('z'))
            ch = static_cast<char>(c - ('a' - 'A'));
    }
    return name;
}

bool ParseTrackFile(const std::vector<std::uint8_t>& bytes,
                    TrackFileData& out,
                    std::string* error) {
    out={}; Reader r(bytes);
    if(!r.Tag5("CDTRK")) return Fail(error,"Invalid track file magic");

    if(!r.CString256(out.author)) return Fail(error,"Invalid track author string");
    if(!r.U8(out.flags)) return Fail(error,"Truncated track flags");

    std::uint16_t fieldCount=0;
    if(!r.U16(fieldCount)) return Fail(error,"Truncated track field-file count");
    if(fieldCount>256) return Fail(error,"Too many track field files");
    out.fieldFiles.resize(fieldCount);
    for(auto& s:out.fieldFiles)
        if(!r.CString256(s)) return Fail(error,"Invalid track field-file string");

    if(!r.U16(out.NumFieldsX)||!r.U16(out.NumFieldsY)) return Fail(error,"Truncated track dimensions");

    if(out.NumFieldsX<=2 || out.NumFieldsX>45 || out.NumFieldsY<=2 || out.NumFieldsY>45)
        return Fail(error,"Track dimensions outside 3..45");
    const std::size_t cellCount=static_cast<std::size_t>(out.NumFieldsX)*out.NumFieldsY;
    if(cellCount>2025) return Fail(error,"Track cell count exceeds fixed storage");
    out.cells.resize(cellCount);
    for(auto& c:out.cells) {
        if(!r.U16(c.fieldFileIndex)||!r.U8(c.rotationOrVariant)||!r.U8(c.field31))
            return Fail(error,"Truncated track cells");

        if (c.fieldFileIndex >= 256)
            return Fail(error,"Track cell field-file index exceeds fixed storage");
        if(c.fieldFileIndex>=out.fieldFiles.size())
            out.fieldFiles.resize(static_cast<std::size_t>(c.fieldFileIndex) + 1u);
    }
    if (out.fieldFiles.size() != fieldCount)
        out.declaredFieldFileCount = fieldCount;

    std::uint16_t dynamicCount=0;
    if(!r.U16(dynamicCount)) return Fail(error,"Truncated track dynamic-file count");
    if(dynamicCount>256) return Fail(error,"Too many track dynamic files");
    out.dynamicFiles.resize(dynamicCount);
    for(auto& s:out.dynamicFiles)
        if(!r.CString256(s)) return Fail(error,"Invalid track dynamic-file string");

    std::uint16_t dropCount=0;
    if(!r.U16(dropCount)) return Fail(error,"Truncated track drop-object count");
    if(dropCount>150) return Fail(error,"Too many track drop objects");
    out.NumDropObjects = dropCount;
    out.dropObjects.resize(dropCount);
    for(auto& d:out.dropObjects) {
        if(!r.U16(d.dynamicFileIndex)||!r.F32(d.position.x)||!r.F32(d.position.y)||
           !r.F32(d.position.z)||!r.F32(d.rotation))
            return Fail(error,"Truncated track drop objects");
        if(d.dynamicFileIndex>=out.dynamicFiles.size())
            return Fail(error,"Track drop-object dynamic index out of range");
    }

    for(auto& s:out.pcOpponentStarts) {
        if(!r.F32(s.position.x)||!r.F32(s.position.y)||!r.F32(s.position.z)||!r.F32(s.rotation))
            return Fail(error,"Truncated track opponent starts");
    }

    out.consumedBytes=r.Pos();
    if(error) error->clear();
    return true;
}

bool BuildTrackFileBytes(const TrackFileData& track,
                         std::vector<std::uint8_t>& out,
                         std::string* error) {

    if (track.fieldFiles.size() > 256)
        return Fail(error, "Too many track field files");
    const std::uint16_t declaredFieldCount = Track_FieldFileCount(track);
    if (declaredFieldCount > track.fieldFiles.size())
        return Fail(error, "Declared track field-file count exceeds initialized storage");
    if (track.dynamicFiles.size() > 256)
        return Fail(error, "Too many track dynamic files");
    if (track.NumDropObjects != track.dropObjects.size())
        return Fail(error, "Track drop-object count does not match storage");
    if (track.NumDropObjects > 150)
        return Fail(error, "Too many track drop objects");
    const std::size_t expectedCells = static_cast<std::size_t>(track.NumFieldsX) * track.NumFieldsY;
    if (track.cells.size() != expectedCells || track.cells.size() > 2025)
        return Fail(error, "Track cell storage does not match dimensions");

    out.clear();
    out.insert(out.end(), {'C','D','T','R','K'});
    AppendCString(out, track.author);
    out.push_back(track.flags);

    AppendU16(out, declaredFieldCount);
    for (std::size_t i = 0; i < declaredFieldCount; ++i)
        AppendCString(out, track.fieldFiles[i]);

    AppendU16(out, track.NumFieldsX);
    AppendU16(out, track.NumFieldsY);
    for (const TrackCell& cell : track.cells) {
        AppendU16(out, cell.fieldFileIndex);
        out.push_back(cell.rotationOrVariant);
        out.push_back(cell.field31);
    }

    AppendU16(out, static_cast<std::uint16_t>(track.dynamicFiles.size()));
    for (const std::string& file : track.dynamicFiles)
        AppendCString(out, file);

    AppendU16(out, track.NumDropObjects);
    for (const TrackDropObject& drop : track.dropObjects) {

        AppendU16(out, drop.dynamicFileIndex);
        AppendF32(out, drop.position.x);
        AppendF32(out, drop.position.y);
        AppendF32(out, drop.position.z);
        AppendF32(out, drop.rotation);
    }

    for (const CARSTART& start : track.pcOpponentStarts) {
        AppendF32(out, start.position.x);
        AppendF32(out, start.position.y);
        AppendF32(out, start.position.z);
        AppendF32(out, start.rotation);
    }

    if (error) error->clear();
    return true;
}

std::string BuildTrackSavePath(const TrackFileData& track) {
    std::string name = track.trackName;
    for (char& ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= static_cast<unsigned char>('A') && c <= static_cast<unsigned char>('Z'))
            ch = static_cast<char>(c + ('a' - 'A'));
    }
    return std::string("TRKDATA/TRACKS/") + name + ".trk";
}

std::uint16_t Track_FieldFileCount(const TrackFileData& track) {
    if (track.declaredFieldFileCount.has_value())
        return *track.declaredFieldFileCount;
    return static_cast<std::uint16_t>(std::min<std::size_t>(track.fieldFiles.size(), 0xffffu));
}

namespace {
std::int32_t NativePositiveMod(TrackRandomSource& random, std::int32_t divisor) {

    return random.Next() % divisor;
}

void EnsureFieldSlot(TrackFileData& track, std::size_t index, std::string_view value) {
    if (track.fieldFiles.size() <= index)
        track.fieldFiles.resize(index + 1u);
    track.fieldFiles[index] = std::string(value);
}

void SetGeneratedTile(TrackCell& cell, std::uint16_t fileIndex, std::uint8_t rotation) {
    cell.fieldFileIndex = fileIndex;
    cell.rotationOrVariant = rotation;
}
}

bool Track_PaintHills(TrackFileData& track,
                                std::int32_t repetitions,
                                std::int32_t level,
                                std::int16_t maxSize,
                                TrackRandomSource& random,
                                std::string* error) {

    if (level != 1 && level != 2)
        return Fail(error, "Track hill level must be 1 or 2");
    if (maxSize < 2)
        return Fail(error, "Track hill max size must be at least 2");
    if (track.NumFieldsX <= 2 || track.NumFieldsY <= 2)
        return Fail(error, "Track dimensions too small for hill generation");
    const std::size_t expected = static_cast<std::size_t>(track.NumFieldsX) * track.NumFieldsY;
    if (track.cells.size() != expected)
        return Fail(error, "Track cell storage does not match dimensions");

    for (std::int32_t repetition = 0; repetition < repetitions; ++repetition) {
        std::int16_t width = static_cast<std::int16_t>(NativePositiveMod(random, maxSize));
        std::int16_t height = static_cast<std::int16_t>(NativePositiveMod(random, maxSize));

        if (level == 1) {
            if (width < 2) width = 2;
            if (height < 2) height = 2;
        } else {

            const std::int16_t minimum = NativePositiveMod(random, 2) == 0 ? 4 : 5;
            if (width < minimum) width = minimum;
            if (height < minimum) height = minimum;
        }

        if (width > static_cast<std::int16_t>(track.NumFieldsX - 2))
            width = static_cast<std::int16_t>(track.NumFieldsX - 2);
        if (height > static_cast<std::int16_t>(track.NumFieldsY - 2))
            height = static_cast<std::int16_t>(track.NumFieldsY - 2);

        std::int16_t startX = static_cast<std::int16_t>(NativePositiveMod(random, track.NumFieldsX - 2) + 1);
        std::int16_t startY = static_cast<std::int16_t>(NativePositiveMod(random, track.NumFieldsY - 2) + 1);
        if (startX + width > static_cast<std::int16_t>(track.NumFieldsX - 1))
            startX = static_cast<std::int16_t>(track.NumFieldsX - 1 - width);
        if (startY + height > static_cast<std::int16_t>(track.NumFieldsY - 1))
            startY = static_cast<std::int16_t>(track.NumFieldsY - 1 - height);

        const std::int16_t endX = static_cast<std::int16_t>(startX + width);
        const std::int16_t endY = static_cast<std::int16_t>(startY + height);
        for (std::int16_t y = startY; y < endY; ++y) {
            for (std::int16_t x = startX; x < endX; ++x) {
                TrackCell& cell = track.cells[static_cast<std::size_t>(y) * track.NumFieldsX + x];
                if (level == 2 && y > startY && y < endY - 1 &&
                    x > startX && x < endX - 1) {
                    cell.field31 = 10;
                } else if (cell.field31 < 5) {
                    cell.field31 = 5;
                }
            }
        }
    }

    if (error) error->clear();
    return true;
}

void Track_RebuildHills(TrackFileData& track) {

    if (track.NumFieldsX <= 2 || track.NumFieldsY <= 2)
        return;
    if (track.cells.size() < static_cast<std::size_t>(track.NumFieldsX) * track.NumFieldsY)
        return;

    for (std::uint16_t y = 1; y + 1 < track.NumFieldsY; ++y) {
        for (std::uint16_t x = 1; x + 1 < track.NumFieldsX; ++x) {
            const auto value = [&](int dx, int dy) -> float {
                return static_cast<float>(track.cells[
                    static_cast<std::size_t>(static_cast<int>(y) + dy) * track.NumFieldsX +
                    static_cast<std::size_t>(static_cast<int>(x) + dx)].field31);
            };
            TrackCell& out = track.cells[static_cast<std::size_t>(y) * track.NumFieldsX + x];
            const float C  = value( 0,  0);
            const float L  = value(-1,  0);
            const float R  = value( 1,  0);
            const float T  = value( 0, -1);
            const float B  = value( 0,  1);
            const float TL = value(-1, -1);
            const float TR = value( 1, -1);
            const float BL = value(-1,  1);
            const float BR = value( 1,  1);

            if (C == L) goto L_426647;
L_42658b:
            if (C > L || C > R || C > T || C > B || C <= TL ||
                C > TR || C > BL || C <= BR) goto L_4266c3;
            SetGeneratedTile(out, 6, 1); goto L_done;
L_426647:
            if (C != R || C != T || C != B || C != TL || C != TR || C != BL)
                goto L_42658b;
            if (C == BR) goto L_done;
            goto L_42658b;
L_4266c3:
            if (C > L || C > R || C > T || C > B || C > TL ||
                C <= TR || C <= BL || C > BR) goto L_42676b;
            SetGeneratedTile(out, 6, 0); goto L_done;
L_42676b:
            if (C <= L) goto L_4267d7;
L_426778:
            if (C > R) goto L_4267e4;
            if (C <= T) goto L_426854;
L_426796:
            if (C > B) goto L_4267e4;
            SetGeneratedTile(out, 4, 2); goto L_done;
L_4267d7:
            if (C > BL) goto L_426778;
L_4267e4:
            if (C > L) goto L_426877;
            if (C <= R) goto L_42686a;
L_426802:
            if (C <= T) goto L_4268e3;
L_426813:
            if (C > B) goto L_426877;
            SetGeneratedTile(out, 4, 3); goto L_done;
L_426854:
            if (C > TR) goto L_426796;
            goto L_4267e4;
L_42686a:
            if (C > BR) goto L_426802;
L_426877:
            if (C <= L) goto L_4268f6;
L_426884:
            if (C > R || C > T) goto L_426903;
            if (C <= B) goto L_426973;
L_4268af:
            SetGeneratedTile(out, 4, 1); goto L_done;
L_4268e3:
            if (C > TL) goto L_426813;
            goto L_426877;
L_4268f6:
            if (C > TL) goto L_426884;
L_426903:
            if (C > L) goto L_426996;
            if (C <= R) goto L_426989;
L_426921:
            if (C > T) goto L_426996;
            if (C <= B) goto L_4269f1;
L_42693f:
            SetGeneratedTile(out, 4, 0); goto L_done;
L_426973:
            if (C > BR) goto L_4268af;
            goto L_426903;
L_426989:
            if (C > TR) goto L_426921;
L_426996:
            if (C > L || C > T || C <= TL) goto L_426a04;
            SetGeneratedTile(out, 5, 1); goto L_done;
L_4269f1:
            if (C > BL) goto L_42693f;
            goto L_426996;
L_426a04:
            if (C > R || C > T || C <= TR) goto L_426a5f;
            SetGeneratedTile(out, 5, 2); goto L_done;
L_426a5f:
            if (C > L || C > B || C <= BL) goto L_426aba;
            SetGeneratedTile(out, 5, 0); goto L_done;
L_426aba:
            if (C > R || C > B || C <= BR) goto L_426b15;
            SetGeneratedTile(out, 5, 3); goto L_done;
L_426b15:
            if (C <= L || C > R || C > T || C > B) goto L_426b7d;
            SetGeneratedTile(out, 3, 1); goto L_done;
L_426b7d:
            if (C > L || C <= R || C > T || C > B) goto L_426be5;
            SetGeneratedTile(out, 3, 3); goto L_done;
L_426be5:
            if (C > L || C > R || C <= T || C > B) goto L_426c4d;
            SetGeneratedTile(out, 3, 2); goto L_done;
L_426c4d:
            if (C > L || C > R || C > T || C <= B) goto L_done;
            SetGeneratedTile(out, 3, 0);
L_done:
            ;
        }
    }
}

bool Track_CreateNewData(TrackFileData& track,
                         std::int16_t fieldsX,
                         std::int16_t fieldsY,
                         TrackGenerationStyle style,
                         TrackRandomSource& random,
                         std::string* error) {

    if (fieldsX <= 2 || fieldsX > 45 || fieldsY <= 2 || fieldsY > 45)
        return Fail(error, "Track dimensions outside 3..45");
    const std::uint8_t styleValue = static_cast<std::uint8_t>(style);
    if (styleValue > 2)
        return Fail(error, "Track generation style outside 0..2");

    track.trackName = "UNNAMED";
    track.author = "AUTHOR";
    track.flags = 0x0f;
    track.fieldFiles.clear();
    track.declaredFieldFileCount = 0;
    track.dynamicFiles.clear();
    track.dropObjects.clear();
    track.NumDropObjects = 0;
    track.NumFieldsX = static_cast<std::uint16_t>(fieldsX);
    track.NumFieldsY = static_cast<std::uint16_t>(fieldsY);
    track.cells.assign(static_cast<std::size_t>(fieldsX) * fieldsY, TrackCell{});
    track.consumedBytes = 0;

    for (CARSTART& start : track.pcOpponentStarts) {
        const std::int32_t xSpan = (fieldsX - 2) * 20;
        const std::int32_t zSpan = (fieldsY - 2) * 20;
        start.position.x = static_cast<float>(NativePositiveMod(random, xSpan) + 20);
        start.position.y = 0.0f;
        start.position.z = static_cast<float>(-20 - NativePositiveMod(random, zSpan));
        start.rotation = static_cast<float>(NativePositiveMod(random, 256));
    }

    track.declaredFieldFileCount = 3;
    EnsureFieldSlot(track, 0, "field.cfl");
    EnsureFieldSlot(track, 1, "border1.cfl");
    EnsureFieldSlot(track, 2, "border2.cfl");
    for (TrackCell& cell : track.cells) {
        cell.fieldFileIndex = 0;
        cell.rotationOrVariant = static_cast<std::uint8_t>(NativePositiveMod(random, 4));
        cell.field31 = 0;
    }

    for (std::uint16_t x = 1; x + 1 < track.NumFieldsX; ++x) {
        SetGeneratedTile(track.cells[x], 1, 1);
        SetGeneratedTile(track.cells[(static_cast<std::size_t>(track.NumFieldsY) - 1u) * track.NumFieldsX + x], 1, 3);
    }
    for (std::uint16_t y = 1; y + 1 < track.NumFieldsY; ++y) {
        SetGeneratedTile(track.cells[static_cast<std::size_t>(y) * track.NumFieldsX], 1, 0);
        SetGeneratedTile(track.cells[static_cast<std::size_t>(y) * track.NumFieldsX + track.NumFieldsX - 1u], 1, 2);
    }
    SetGeneratedTile(track.cells[0], 2, 0);
    SetGeneratedTile(track.cells[track.NumFieldsX - 1u], 2, 1);
    SetGeneratedTile(track.cells[(static_cast<std::size_t>(track.NumFieldsY) - 1u) * track.NumFieldsX], 2, 3);
    SetGeneratedTile(track.cells[static_cast<std::size_t>(track.NumFieldsY) * track.NumFieldsX - 1u], 2, 2);

    if (styleValue != 0) {

        track.declaredFieldFileCount = static_cast<std::uint16_t>(
            Track_FieldFileCount(track) + 3u);
        EnsureFieldSlot(track, 3, "mount1.cfl");
        EnsureFieldSlot(track, 4, "mount2.cfl");
        EnsureFieldSlot(track, 5, "mount3.cfl");
        EnsureFieldSlot(track, 6, "mount4.cfl");

        if (styleValue == 1) {
            if (!Track_PaintHills(track,
                    static_cast<std::int32_t>(track.cells.size() / 50u), 1, 8, random, error))
                return false;
        } else {
            if (!Track_PaintHills(track,
                    static_cast<std::int32_t>(track.cells.size() / 50u), 2, 8, random, error))
                return false;
            if (!Track_PaintHills(track,
                    static_cast<std::int32_t>(track.cells.size() / 20u), 1, 5, random, error))
                return false;
        }
        Track_RebuildHills(track);
    }

    if (error) error->clear();
    return true;
}

TrackFieldKind GetFieldConfigType(const std::string& name) {

    if(name=="mount1.cfl") return TrackFieldKind::Mount1;
    if(name=="mount2.cfl") return TrackFieldKind::Mount2;
    if(name=="mount3.cfl") return TrackFieldKind::Mount3;
    if(name=="mount4.cfl") return TrackFieldKind::Mount4;
    if(name=="mountstr.cfl") return TrackFieldKind::MountStreet;
    if(name=="mounthw.cfl") return TrackFieldKind::MountHighway;
    if(name=="mounrail.cfl") return TrackFieldKind::MountRail;
    return TrackFieldKind::Generic;
}

float TrackFieldYOffset(const TrackCell& cell, TrackFieldKind kind) {
    switch (kind) {
    case TrackFieldKind::Mount1:
    case TrackFieldKind::Mount2:
    case TrackFieldKind::Mount3:
    case TrackFieldKind::Mount4:
    case TrackFieldKind::MountStreet:
    case TrackFieldKind::MountHighway:
    case TrackFieldKind::MountRail:
        return static_cast<float>(static_cast<int>(cell.field31) - 5);
    case TrackFieldKind::Generic:
        return static_cast<float>(cell.field31);
    }
    return static_cast<float>(cell.field31);
}

bool BuildTrackPlacement(const TrackCell& cell,
                              std::uint16_t gridX,
                              std::uint16_t gridY,
                              const std::string& fieldConfigName,
                              float modelHeight,
                              FieldObjectPlacement& out,
                              std::string* error) {
    const auto kind = GetFieldConfigType(fieldConfigName);
    return BuildFieldPlacement(static_cast<std::int16_t>(gridX),
                                     static_cast<std::int16_t>(gridY),
                                     cell.rotationOrVariant,
                                     TrackFieldYOffset(cell, kind),
                                     0.0f,
                                     modelHeight,
                                     out,
                                     error);
}

bool BuildTrackFields(const TrackFileData& track,
                                  const std::vector<float>& modelHeights,
                                  std::vector<TrackFieldSpawn>& out,
                                  std::string* error) {
    out.clear();
    if (modelHeights.size() != track.fieldFiles.size())
        return Fail(error, "Track field model-height table size mismatch");
    const std::size_t expected = static_cast<std::size_t>(track.NumFieldsX) * track.NumFieldsY;
    if (track.cells.size() != expected)
        return Fail(error, "Track cell array size mismatch");
    out.reserve(track.cells.size());
    for (std::uint16_t y = 0; y < track.NumFieldsY; ++y) {
        for (std::uint16_t x = 0; x < track.NumFieldsX; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * track.NumFieldsX + x;
            const TrackCell& cell = track.cells[index];
            if (cell.fieldFileIndex >= track.fieldFiles.size())
                return Fail(error, "Track field-file index out of range while associating");
            TrackFieldSpawn plan;
            plan.cellIndex = index;
            plan.fieldFileIndex = cell.fieldFileIndex;

            plan.objectName = std::string("trackfield") + std::to_string(index);
            plan.fieldConfigName = track.fieldFiles[cell.fieldFileIndex];
            plan.association = GetFieldConfigType(plan.fieldConfigName);
            if (!BuildTrackPlacement(cell, x, y, plan.fieldConfigName,
                                          modelHeights[cell.fieldFileIndex],
                                          plan.placement, error))
                return false;
            out.push_back(std::move(plan));
        }
    }
    if (error) error->clear();
    return true;
}

bool BuildTrackDrops(const TrackFileData& track,
                                 std::vector<TrackDropSpawn>& out,
                                 std::string* error) {

    out.clear();
    if (track.NumDropObjects > 150)
        return Fail(error, "Track drop-object count exceeds fixed runtime storage");
    out.reserve(track.NumDropObjects);
    for (std::size_t i = 0; i < track.NumDropObjects; ++i) {
        const TrackDropObject& drop = track.dropObjects[i];
        if (drop.dynamicFileIndex >= track.dynamicFiles.size())
            return Fail(error, "Track drop-object dynamic index out of range while associating");

        TrackDropSpawn plan;
        plan.dropIndex = i;
        plan.dynamicFileIndex = drop.dynamicFileIndex;

        plan.objectName = std::string("dropobject") + std::to_string(i);
        plan.dynamicConfigName = track.dynamicFiles[drop.dynamicFileIndex];
        plan.position = drop.position;
        plan.rotation = drop.rotation;
        out.push_back(std::move(plan));
    }
    if (error) error->clear();
    return true;
}

FieldFileResult Track_ReplaceFieldFile(
    TrackFileData& track,
    std::string_view newFile,
    std::string_view previousFile) {
    FieldFileResult result;

    const std::uint16_t declaredCount = Track_FieldFileCount(track);
    if (declaredCount > track.fieldFiles.size())
        return result;

    auto findField = [&](std::string_view name) -> std::int16_t {

        for (std::size_t i = 0; i < declaredCount; ++i) {
            if (track.fieldFiles[i] == name)
                return static_cast<std::int16_t>(i);
        }
        return -1;
    };

    std::int16_t newId = findField(newFile);
    const std::int16_t previousId = findField(previousFile);
    if (previousId < 0) {

        result.badPreviousId = true;
        return result;
    }

    std::uint16_t previousReferences = 0;
    for (const TrackCell& cell : track.cells) {
        if (cell.fieldFileIndex == static_cast<std::uint16_t>(previousId))
            ++previousReferences;
    }

    if (newId == previousId) {
        result.applied = true;
        result.resultingIndex = newId;
        return result;
    }

    if (previousReferences > 1) {

        if (newId >= 0) {
            result.applied = true;
            result.resultingIndex = newId;
            return result;
        }

        if (declaredCount == 255)
            result.hitLimit = true;
        if (declaredCount >= 256)
            return result;

        const std::size_t appended = declaredCount;
        if (track.fieldFiles.size() <= appended)
            track.fieldFiles.emplace_back(newFile);
        else
            track.fieldFiles[appended] = std::string(newFile);
        track.declaredFieldFileCount = static_cast<std::uint16_t>(declaredCount + 1u);
        result.applied = true;
        result.resultingIndex = static_cast<std::int16_t>(appended);
        return result;
    }

    if (newId < 0) {
        track.fieldFiles[static_cast<std::size_t>(previousId)] = std::string(newFile);
        result.applied = true;
        result.resultingIndex = previousId;
        return result;
    }

    const std::size_t previous = static_cast<std::size_t>(previousId);
    const std::size_t last = static_cast<std::size_t>(declaredCount - 1u);
    const std::size_t oldStorageSize = track.fieldFiles.size();
    if (previous < last) {
        track.fieldFiles[previous] = track.fieldFiles[last];
        for (TrackCell& cell : track.cells) {
            if (cell.fieldFileIndex == last)
                cell.fieldFileIndex = static_cast<std::uint16_t>(previous);
        }
        if (static_cast<std::size_t>(newId) == last)
            newId = previousId;
    }
    const std::uint16_t newCount = static_cast<std::uint16_t>(declaredCount - 1u);
    track.declaredFieldFileCount = newCount;

    if (oldStorageSize == declaredCount)
        track.fieldFiles.resize(newCount);

    result.applied = true;
    result.resultingIndex = newId;
    return result;
}

DynamicFileAdd Track_AddDynamicFile(
    TrackFileData& track,
    std::string_view dynamicFile) {
    DynamicFileAdd result;
    for (std::size_t i = 0; i < track.dynamicFiles.size(); ++i) {
        if (track.dynamicFiles[i] == dynamicFile) {
            result.applied = true;
            result.index = static_cast<std::int16_t>(i);
            return result;
        }
    }

    if (track.dynamicFiles.size() == 255)
        result.hitLimit = true;
    if (track.dynamicFiles.size() >= 256)
        return result;

    const std::size_t index = track.dynamicFiles.size();
    track.dynamicFiles.emplace_back(dynamicFile);
    result.applied = true;
    result.index = static_cast<std::int16_t>(index);
    return result;
}

DynamicFileRemove Track_RemoveDynamicFile(
    TrackFileData& track,
    std::string_view previousFile) {
    DynamicFileRemove result;
    std::int16_t previousId = -1;
    for (std::size_t i = 0; i < track.dynamicFiles.size(); ++i) {
        if (track.dynamicFiles[i] == previousFile) {
            previousId = static_cast<std::int16_t>(i);
            break;
        }
    }

    if (previousId < 0) {

        result.badPreviousId = true;
        return result;
    }

    for (const TrackDropObject& drop : track.dropObjects) {
        if (drop.dynamicFileIndex == static_cast<std::uint16_t>(previousId))
            ++result.referenceCount;
    }

    result.completed = true;
    result.removedIndex = previousId;
    if (result.referenceCount > 1)
        return result;

    const std::size_t previous = static_cast<std::size_t>(previousId);
    const std::size_t last = track.dynamicFiles.size() - 1;
    if (previous < last) {
        track.dynamicFiles[previous] = track.dynamicFiles[last];
        for (TrackDropObject& drop : track.dropObjects) {
            if (drop.dynamicFileIndex == last)
                drop.dynamicFileIndex = static_cast<std::uint16_t>(previous);
        }
    }
    track.dynamicFiles.pop_back();
    result.removed = true;
    return result;
}

namespace {

void TrackLowerAsciiInPlace(std::string& text) {

    for (char& ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= static_cast<unsigned char>('A') && c <= static_cast<unsigned char>('Z'))
            ch = static_cast<char>(c + ('a' - 'A'));
    }
}

bool HasAdjustedMount(std::string_view name) {

    return name == "mount1.cfl" || name == "mountstr.cfl" ||
           name == "mounthw.cfl" || name == "mounrail.cfl";
}

bool IsLockedMount(std::string_view name) {
    return name == "mount2.cfl" || name == "mount3.cfl" || name == "mount4.cfl";
}

bool MapMountType(std::string& requested) {
    if (requested == "street.cfl") {
        requested = "mountstr.cfl";
        return true;
    }
    if (requested == "highway.cfl") {
        requested = "mounthw.cfl";
        return true;
    }
    if (requested == "railway1.cfl") {
        requested = "mounrail.cfl";
        return true;
    }
    if (requested == "field.cfl") {
        requested = "mount1.cfl";
        return true;
    }
    return false;
}

bool HaveRuntimeContext(const TrackContext& runtime) {
    return runtime.world != nullptr && runtime.deleter != nullptr && runtime.objects != nullptr;
}

std::uint32_t ValidateTrackCell(std::int16_t x,
                                         std::int16_t y,
                                         const TrackFileData& track) {
    std::uint32_t diagnostics = TrackEditDiag_None;
    if (!(x > 0 && x < static_cast<std::int32_t>(track.NumFieldsX) - 1))
        diagnostics |= TrackEditDiag_XInterior;
    if (!(y > 0 && y < static_cast<std::int32_t>(track.NumFieldsY) - 1))
        diagnostics |= TrackEditDiag_YInterior;
    return diagnostics;
}

bool ValidCellIndex(std::int16_t x,
                    std::int16_t y,
                    const TrackFileData& track,
                    std::size_t& index) {
    if (x < 0 || y < 0 ||
        static_cast<std::uint16_t>(x) >= track.NumFieldsX ||
        static_cast<std::uint16_t>(y) >= track.NumFieldsY)
        return false;
    index = static_cast<std::size_t>(y) * track.NumFieldsX + static_cast<std::size_t>(x);
    return index < track.cells.size();
}

}

TrackGroundResult Track_DropToGround(
    const TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime) {
    TrackGroundResult result;
    if (!HaveRuntimeContext(runtime)) {
        result.diagnostics |= TrackEdit_NoRuntime;
        return result;
    }

    runtime.world->UpdateStaticObjects(runtime.rendererFrameDelta, *runtime.deleter);
    runtime.world->Update(runtime.rendererFrameDelta, *runtime.deleter);

    if (state.dataAssociated != 1) {

        result.diagnostics |= TrackEdit_BadState;
    }

    const std::size_t count = track.NumDropObjects;
    if (count > state.dropObjects.size() || state.dropObjectCount > state.dropObjects.size()) {
        result.diagnostics |= TrackEdit_TooManyDrops;
        return result;
    }

    state.dropObjectCount = count;
    for (std::size_t i = 0; i < count; ++i) {
        CD3DOBJECT* object = state.dropObjects[i];
        if (object == nullptr) {
            result.diagnostics |= TrackEdit_NoDrop;
            return result;
        }

        CD3DVECTOR start = runtime.objects->GetPosition(*object);
        CD3DVECTOR end = start;
        start.y = 100.0f;
        end.y = -10.0f;

        const WorldSegmentHit hit = runtime.world->TraceStaticSegment(start, end);
        if (hit.hit) {
            runtime.objects->SetPosition(*object, hit.point);
            const float halfHeight = runtime.objects->ModelHeight(*object) * 0.5f;
            runtime.objects->Translate(*object, CD3DVECTOR{0.0f, halfHeight, 0.0f});
            ++result.hits;
        } else {
            runtime.objects->SetPosition(*object, end);
        }
        ++result.processed;
    }

    result.completed = true;
    return result;
}

TrackFieldEdit Track_EditField(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t x,
    std::int16_t y,
    std::string& requestedFile,
    std::int8_t rotation) {
    TrackFieldEdit result;
    result.diagnostics |= ValidateTrackCell(x, y, track);
    if (!(rotation >= 0 && rotation <= 3))
        result.diagnostics |= TrackEdit_BadRotation;

    std::size_t cellIndex = 0;
    if (!ValidCellIndex(x, y, track, cellIndex)) {

        result.diagnostics |= TrackEdit_BadCell;
        return result;
    }
    result.cellIndex = cellIndex;

    TrackCell& cell = track.cells[cellIndex];
    if (cell.fieldFileIndex >= track.fieldFiles.size()) {
        result.diagnostics |= TrackEdit_BadCell;
        return result;
    }

    std::string previousFile = track.fieldFiles[cell.fieldFileIndex];
    TrackLowerAsciiInPlace(requestedFile);
    TrackLowerAsciiInPlace(previousFile);
    result.normalizedPreviousFile = previousFile;

    if (IsLockedMount(previousFile)) {
        result.returnCode = 4;
        result.normalizedRequestedFile = requestedFile;
        result.completed = true;
        return result;
    }

    if (HasAdjustedMount(previousFile)) {

        if (!MapMountType(requestedFile)) {
            result.returnCode = 4;
            result.normalizedRequestedFile = requestedFile;
            result.completed = true;
            return result;
        }
        result.preservedRotation = true;
    }
    result.normalizedRequestedFile = requestedFile;

    const std::uint8_t originalRotation = cell.rotationOrVariant;
    const FieldFileResult mutation =
        Track_ReplaceFieldFile(track, requestedFile, previousFile);
    if (!mutation.applied || mutation.resultingIndex < 0) {
        result.diagnostics |= TrackEdit_FieldFileError;
        return result;
    }

    cell.fieldFileIndex = static_cast<std::uint16_t>(mutation.resultingIndex);
    cell.rotationOrVariant = result.preservedRotation
        ? originalRotation
        : static_cast<std::uint8_t>(rotation);

    if (state.dataAssociated == 1) {
        if (!HaveRuntimeContext(runtime)) {
            result.diagnostics |= TrackEdit_NoRuntime;
            return result;
        }
        if (cellIndex >= state.fieldObjects.size() || state.fieldObjects[cellIndex] == nullptr) {
            result.diagnostics |= TrackEdit_NoField;
            return result;
        }

        CD3DOBJECT* oldObject = state.fieldObjects[cellIndex];
        const std::string objectName(oldObject->Name());
        runtime.world->DeleteObjectByName(objectName, *runtime.deleter);

        const std::string& resultingConfig = track.fieldFiles[cell.fieldFileIndex];
        TrackFieldCreateReq create;
        create.objectName = objectName;
        create.fieldConfigName = resultingConfig;
        create.gridX = x;
        create.gridY = y;
        create.rotationOrVariant = cell.rotationOrVariant;
        create.field31 = cell.field31;
        create.mountHeightAdjusted = HasAdjustedMount(resultingConfig);
        create.verticalOffset = create.mountHeightAdjusted
            ? static_cast<float>(static_cast<int>(cell.field31) - 5)
            : static_cast<float>(cell.field31);

        CD3DOBJECT* replacement = runtime.objects->CreateFieldObject(create);
        state.fieldObjects[cellIndex] = replacement;
        result.recreatedRuntimeObject = replacement != nullptr;

        const TrackGroundResult refresh =
            Track_DropToGround(track, state, runtime);
        result.diagnostics |= refresh.diagnostics;
    }

    result.returnCode = 0;
    result.completed = true;
    return result;
}

TrackDropAdd Track_AddDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::string& dynamicFile,
    float x,
    float z,
    float rotation) {
    TrackDropAdd result;

    const float maxX = static_cast<float>((static_cast<int>(track.NumFieldsX) - 1) * 20);
    const float minZ = static_cast<float>(-(static_cast<int>(track.NumFieldsY) - 1) * 20);
    if (std::isnan(x) || !(x > 20.0f && x < maxX))
        result.diagnostics |= TrackEditDiag_XInterior;
    if (!std::isnan(z) && !(z < -20.0f && z > minZ))
        result.diagnostics |= TrackEditDiag_YInterior;

    TrackLowerAsciiInPlace(dynamicFile);
    const DynamicFileAdd config = Track_AddDynamicFile(track, dynamicFile);
    if (!config.applied || config.index < 0) {
        result.diagnostics |= TrackEdit_BadDropConfig;
        return result;
    }
    if (track.NumDropObjects >= state.dropObjects.size()) {

        result.diagnostics |= TrackEdit_DropFull;
        return result;
    }

    TrackDropObject drop;
    drop.dynamicFileIndex = static_cast<std::uint16_t>(config.index);
    drop.position = CD3DVECTOR{x, 1.0f, z};
    drop.rotation = rotation;
    track.dropObjects.push_back(drop);
    track.NumDropObjects = static_cast<std::uint16_t>(track.dropObjects.size());
    const std::size_t id = track.NumDropObjects - 1u;
    state.dropObjectCount = track.NumDropObjects;
    result.id = static_cast<std::int16_t>(id);

    if (state.dataAssociated == 1) {
        if (!HaveRuntimeContext(runtime)) {
            result.diagnostics |= TrackEdit_NoRuntime;
            return result;
        }
        if (state.dropObjects[id] != nullptr)
            result.diagnostics |= TrackEdit_BadDrop;

        TrackDropCreateReq create;
        create.objectName = std::string("dropobject") + std::to_string(id);
        create.dynamicConfigName = track.dynamicFiles[drop.dynamicFileIndex];
        create.position = drop.position;
        create.rotation = drop.rotation;
        CD3DOBJECT* object = runtime.objects->CreateDropObject(create);
        state.dropObjects[id] = object;
        result.runtimeObjectCreated = object != nullptr;
        if (object != nullptr) {
            runtime.objects->SetPosition(*object, drop.position);
            runtime.objects->RotateAround(*object, drop.position, drop.rotation);
        }
        const TrackGroundResult refresh =
            Track_DropToGround(track, state, runtime);
        result.diagnostics |= refresh.diagnostics;
    }

    result.completed = true;
    return result;
}

TrackDropEdit Track_EditDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id,
    float x,
    float z,
    float rotation) {
    TrackDropEdit result;
    if (id < 0 || static_cast<std::size_t>(id) >= track.NumDropObjects) {
        result.diagnostics |= TrackEdit_BadDropId;
        return result;
    }

    TrackDropObject& drop = track.dropObjects[static_cast<std::size_t>(id)];
    const float previousRotation = drop.rotation;
    drop.position.x = x;
    drop.position.y = 0.0f;
    drop.position.z = z;
    drop.rotation = rotation;
    state.dropObjectCount = track.NumDropObjects;

    if (state.dataAssociated == 1) {
        if (!HaveRuntimeContext(runtime)) {
            result.diagnostics |= TrackEdit_NoRuntime;
            return result;
        }
        CD3DOBJECT* object = state.dropObjects[static_cast<std::size_t>(id)];
        if (object == nullptr) {
            result.diagnostics |= TrackEdit_NoDrop;
            return result;
        }
        runtime.objects->SetPosition(*object, drop.position);
        runtime.objects->RotateAround(*object, drop.position, drop.rotation - previousRotation);
        result.runtimeObjectUpdated = true;
        const TrackGroundResult refresh =
            Track_DropToGround(track, state, runtime);
        result.diagnostics |= refresh.diagnostics;
    }

    result.completed = true;
    return result;
}

TrackDropDelete Track_DeleteDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id) {
    TrackDropDelete result;
    if (id < 0 || static_cast<std::size_t>(id) >= track.NumDropObjects) {
        result.diagnostics |= TrackEdit_BadDropId;
        return result;
    }

    const std::size_t index = static_cast<std::size_t>(id);
    if (track.dropObjects[index].dynamicFileIndex >= track.dynamicFiles.size()) {
        result.diagnostics |= TrackEdit_BadDropConfig;
        return result;
    }
    const std::string previousConfig = track.dynamicFiles[track.dropObjects[index].dynamicFileIndex];
    const DynamicFileRemove configRemoval =
        Track_RemoveDynamicFile(track, previousConfig);
    if (!configRemoval.completed) {
        result.diagnostics |= TrackEdit_BadDropConfig;
        return result;
    }

    const std::size_t last = track.NumDropObjects - 1;
    if (index != last) {

        track.dropObjects[index] = track.dropObjects[last];
        result.movedLastRecord = true;
    }

    if (state.dataAssociated == 1) {
        if (!HaveRuntimeContext(runtime)) {
            result.diagnostics |= TrackEdit_NoRuntime;
            return result;
        }
        CD3DOBJECT* doomed = state.dropObjects[index];
        if (doomed == nullptr) {
            result.diagnostics |= TrackEdit_NoDrop;
            return result;
        }
        const std::string doomedName(doomed->Name());
        runtime.world->DeleteObjectByName(doomedName, *runtime.deleter);
        result.runtimeObjectDeleted = true;

        if (index != last) {
            state.dropObjects[index] = state.dropObjects[last];
            state.dropObjects[last] = nullptr;
        } else {
            state.dropObjects[index] = nullptr;
        }
    }

    track.dropObjects.pop_back();
    track.NumDropObjects = static_cast<std::uint16_t>(track.dropObjects.size());
    state.dropObjectCount = track.NumDropObjects;
    result.completed = true;
    return result;
}

StartSetResult Track_SetOpponentStart(
    TrackFileData& track,
    std::int16_t id,
    float x,
    float z,
    float rotation) {
    StartSetResult result;
    if (id < 0 || id >= static_cast<std::int16_t>(track.pcOpponentStarts.size())) {
        result.diagnostics |= OpponentStart_BadId;
        return result;
    }

    const float maxX = static_cast<float>((static_cast<int>(track.NumFieldsX) - 1) * 20);
    const float minZ = static_cast<float>(-(static_cast<int>(track.NumFieldsY) - 1) * 20);
    if (std::isnan(x) || !(x > 20.0f && x < maxX))
        result.diagnostics |= OpponentStart_BadX;
    if (!std::isnan(z) && !(z < -20.0f && z > minZ))
        result.diagnostics |= OpponentStart_BadZ;

    CARSTART& start = track.pcOpponentStarts[static_cast<std::size_t>(id)];
    start.position.x = x;
    start.position.y = 0.0f;
    start.position.z = z;
    start.rotation = rotation;
    result.completed = true;
    return result;
}

StartGroundResult Track_UpdateStartGround(
    TrackFileData& track,
    const TrackSceneState& state,
    TrackContext runtime) {
    StartGroundResult result;
    if (runtime.world == nullptr || runtime.deleter == nullptr) {
        result.diagnostics |= OpponentStart_NoRuntime;
        return result;
    }

    runtime.world->UpdateStaticObjects(runtime.rendererFrameDelta, *runtime.deleter);
    runtime.world->Update(runtime.rendererFrameDelta, *runtime.deleter);

    if (state.dataAssociated != 1)
        result.diagnostics |= OpponentStart_BadState;

    for (CARSTART& start : track.pcOpponentStarts) {
        CD3DVECTOR high = start.position;
        CD3DVECTOR low = start.position;
        high.y = 100.0f;
        low.y = -10.0f;
        const WorldSegmentHit hit = runtime.world->TraceStaticSegment(high, low);
        if (hit.hit) {

            start.position.y = hit.point.y;
            ++result.hits;
        } else {
            start.position.y = 0.0f;
        }
    }

    result.completed = true;
    return result;
}

StartLookup Track_GetOpponentStart(
    TrackFileData& track,
    const TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id) {
    StartLookup result;
    if (id < 0 || id >= static_cast<std::int16_t>(track.pcOpponentStarts.size())) {
        result.diagnostics |= OpponentStart_BadId;
        return result;
    }

    if (state.dataAssociated == 1) {
        const StartGroundResult refresh =
            Track_UpdateStartGround(track, state, runtime);
        result.diagnostics |= refresh.diagnostics;
        if (!refresh.completed)
            return result;
        result.refreshedAllStarts = true;
    }

    result.start = &track.pcOpponentStarts[static_cast<std::size_t>(id)];
    result.completed = true;
    return result;
}

void Track_CopyData(const TrackFileData& source, TrackFileData& destination) {

    destination.trackName = source.trackName;
    destination.author = source.author;

    destination.flags = source.flags;
    const std::uint16_t sourceFieldCount = Track_FieldFileCount(source);
    if (destination.fieldFiles.size() < sourceFieldCount)
        destination.fieldFiles.resize(sourceFieldCount);
    const std::size_t copyFieldCount = std::min<std::size_t>(
        sourceFieldCount, source.fieldFiles.size());
    for (std::size_t i = 0; i < copyFieldCount; ++i)
        destination.fieldFiles[i] = source.fieldFiles[i];

    destination.declaredFieldFileCount = sourceFieldCount;
    destination.NumFieldsX = source.NumFieldsX;
    destination.NumFieldsY = source.NumFieldsY;
    destination.cells = source.cells;
    destination.dynamicFiles = source.dynamicFiles;
    destination.dropObjects = source.dropObjects;
    destination.NumDropObjects = source.NumDropObjects;
    destination.pcOpponentStarts = source.pcOpponentStarts;

}

std::uint8_t Track_IsAssociated(const TrackSceneState& state) {
    return state.dataAssociated;
}

TrackFieldRef Track_GetFieldObject(
    const TrackFileData& track,
    const TrackSceneState& state,
    std::int16_t x,
    std::int16_t y) {
    TrackFieldRef result;

    if (state.dataAssociated != 1)
        result.diagnostics |= TrackRuntime_BadState;

    if (x < 0 || x >= static_cast<std::int16_t>(track.NumFieldsX)) {
        result.diagnostics |= TrackRuntime_BadX;
        return result;
    }
    if (y < 0 || y >= static_cast<std::int16_t>(track.NumFieldsY)) {
        result.diagnostics |= TrackRuntime_BadY;
        return result;
    }

    const std::size_t index = static_cast<std::size_t>(static_cast<std::uint16_t>(y)) *
                              static_cast<std::size_t>(track.NumFieldsX) +
                              static_cast<std::size_t>(static_cast<std::uint16_t>(x));
    result.cellIndex = index;
    if (index >= state.fieldObjects.size()) {
        result.diagnostics |= TrackRuntime_BadField;
        return result;
    }

    result.object = state.fieldObjects[index];
    result.completed = true;
    return result;
}

TrackDropRef Track_GetDropObject(
    const TrackFileData& track,
    const TrackSceneState& state,
    std::int16_t id) {
    TrackDropRef result;

    if (state.dataAssociated != 1)
        result.diagnostics |= TrackRuntime_BadState;

    if (id < 0 || static_cast<std::size_t>(id) >= track.NumDropObjects) {
        result.diagnostics |= TrackRuntime_BadDrop;
        return result;
    }

    result.object = state.dropObjects[static_cast<std::size_t>(id)];
    result.completed = true;
    return result;
}

TrackDetachResult Track_Deassociate3DData(
    TrackSceneState& state,
    WorldState& world,
    WorldDelete& deleter) {
    TrackDetachResult result;
    if (state.dataAssociated != 1)
        result.diagnostics |= TrackDetach_BadState;

    for (CD3DOBJECT*& object : state.fieldObjects) {
        if (object == nullptr) {
            result.diagnostics |= TrackDetach_NullObject;
            return result;
        }
        const std::string name(object->Name());
        ++result.fieldDeleteAttempts;
        if (world.DeleteObjectByName(name, deleter))
            ++result.fieldDeleteSuccesses;
        object = nullptr;
    }
    if (!state.fieldObjects.empty()) {
        state.fieldObjects.clear();
        state.fieldObjects.shrink_to_fit();
        result.fieldPointerArrayFreed = true;
    }

    if (state.dropObjectCount > state.dropObjects.size()) {
        result.diagnostics |= TrackDetach_TooManyDrops;
        return result;
    }
    for (std::size_t i = 0; i < state.dropObjectCount; ++i) {
        CD3DOBJECT*& object = state.dropObjects[i];
        if (object == nullptr) {
            result.diagnostics |= TrackDetach_NullObject;
            return result;
        }
        const std::string name(object->Name());
        ++result.dropDeleteAttempts;
        if (world.DeleteObjectByName(name, deleter))
            ++result.dropDeleteSuccesses;
        object = nullptr;
    }

    state.dataAssociated = 0;
    result.completed = true;
    return result;
}

TrackLifeResult Track_InitState(
    TrackFileData& track,
    TrackSceneState& live,
    std::int16_t fieldsX,
    std::int16_t fieldsY,
    TrackGenerationStyle style,
    TrackRandomSource& random,
    std::string* error) {

    track = TrackFileData{};
    live = TrackSceneState{};

    TrackLifeResult result;
    if (!Track_CreateNewData(track, fieldsX, fieldsY, style, random, error)) {
        result.diagnostics |= TrackLife_GenFailed;
        return result;
    }
    result.completed = true;
    return result;
}

TrackLifeResult Track_NewTrack(
    TrackFileData& track,
    TrackSceneState& live,
    TrackContext runtime,
    std::int16_t fieldsX,
    std::int16_t fieldsY,
    TrackGenerationStyle style,
    TrackRandomSource& random,
    std::string* error) {
    TrackLifeResult result;

    if (live.dataAssociated == 1) {
        if (runtime.world == nullptr || runtime.deleter == nullptr) {
            result.diagnostics |= TrackLife_NoRuntime;
            return result;
        }
        const TrackDetachResult deassociated =
            Track_Deassociate3DData(live, *runtime.world, *runtime.deleter);
        if (!deassociated.completed) {
            result.diagnostics |= TrackLife_DetachFailed;
            return result;
        }
        result.deassociated = true;
    }

    if (!Track_CreateNewData(track, fieldsX, fieldsY, style, random, error)) {
        result.diagnostics |= TrackLife_GenFailed;
        return result;
    }
    result.completed = true;
    return result;
}

TrackLifeResult Track_PrepareDestroy(
    TrackSceneState& live,
    TrackContext runtime) {
    TrackLifeResult result;

    if (live.dataAssociated == 1) {
        if (runtime.world == nullptr || runtime.deleter == nullptr) {
            result.diagnostics |= TrackLife_NoRuntime;
            return result;
        }
        const TrackDetachResult deassociated =
            Track_Deassociate3DData(live, *runtime.world, *runtime.deleter);
        if (!deassociated.completed) {
            result.diagnostics |= TrackLife_DetachFailed;
            return result;
        }
        result.deassociated = true;
    }
    result.completed = true;
    return result;
}

TrackReplaceResult Track_ReplaceData(
    TrackFileData& current,
    TrackSceneState& live,
    TrackContext runtime,
    const TrackFileData& candidate,
    const TrackResourceProbe& resources,
    std::string& missingList) {
    TrackReplaceResult result;
    missingList.clear();

    TrackFileData backup;
    Track_CopyData(current, backup);

    if (live.dataAssociated == 1) {
        if (runtime.world == nullptr || runtime.deleter == nullptr) {
            result.failed = true;
            result.diagnostics |= TrackReplace_NoRuntime;
            return result;
        }
        const auto deassociated = Track_Deassociate3DData(live, *runtime.world, *runtime.deleter);
        if (!deassociated.completed) {
            result.failed = true;
            result.diagnostics |= TrackReplace_DetachError;
            return result;
        }
        result.deassociated = true;
    }

    Track_CopyData(candidate, current);

    const auto validateTable = [&](std::string_view root, const std::vector<std::string>& files) {
        for (const std::string& file : files) {
            std::string path(root);
            path += file;
            if (!resources.Exists(path)) {
                result.missingFiles.push_back(file);
                missingList += file;
                missingList.push_back('|');
            }
        }
    };
    {
        const std::uint16_t count = Track_FieldFileCount(current);
        std::vector<std::string> declaredFields(current.fieldFiles.begin(),
            current.fieldFiles.begin() + std::min<std::size_t>(count, current.fieldFiles.size()));
        validateTable("TRKDATA/FIELDS/", declaredFields);
    }
    validateTable("TRKDATA/DYNAMICS/", current.dynamicFiles);

    if (missingList.empty()) {
        result.failed = false;
        return result;
    }

    Track_CopyData(backup, current);
    result.rolledBack = true;
    bool rollbackValid = true;
    for (std::size_t i = 0; i < std::min<std::size_t>(
             Track_FieldFileCount(current), current.fieldFiles.size()); ++i)
        rollbackValid = resources.Exists(std::string("TRKDATA/FIELDS/") + current.fieldFiles[i]) && rollbackValid;
    for (const std::string& file : current.dynamicFiles)
        rollbackValid = resources.Exists(std::string("TRKDATA/DYNAMICS/") + file) && rollbackValid;
    if (!rollbackValid)
        result.diagnostics |= TrackReplace_Rollback;

    result.failed = true;
    return result;
}

TrackLoadResult Track_LoadBytes(
    TrackFileData& current,
    TrackSceneState& live,
    TrackContext runtime,
    std::string_view requestedFilename,
    const std::vector<std::uint8_t>& bytes,
    const TrackResourceProbe& resources,
    std::string& missingList) {
    TrackLoadResult result;
    result.relativeTrackPath = BuildTrackPath(requestedFilename);

    missingList.clear();

    TrackFileData backup;
    Track_CopyData(current, backup);

    if (live.dataAssociated == 1) {
        if (runtime.world == nullptr || runtime.deleter == nullptr) {
            result.diagnostics |= TrackLoad_NoRuntime;
            result.failed = true;
            return result;
        }
        const auto deassociated = Track_Deassociate3DData(live, *runtime.world, *runtime.deleter);
        if (!deassociated.completed) {
            result.diagnostics |= TrackLoad_DetachFailed;
            result.failed = true;
            return result;
        }
        result.deassociated = true;
    }

    if (requestedFilename.size() < 4) {
        result.diagnostics |= TrackLoad_ShortFilename;
        Track_CopyData(backup, current);
        result.rolledBack = true;
        result.failed = true;
        return result;
    }
    result.derivedTrackName = DeriveTrackLoadName(requestedFilename);

    static constexpr char kMagic[5] = {'C','D','T','R','K'};
    if (bytes.size() < 5 || !std::equal(std::begin(kMagic), std::end(kMagic), bytes.begin()))
        result.diagnostics |= TrackLoad_BadMagic;

    TrackFileData loaded;
    std::string parseError;
    if (!ParseTrackFile(bytes, loaded, &parseError)) {

        result.diagnostics |= TrackLoad_ParseError;
        Track_CopyData(backup, current);
        result.rolledBack = true;
        result.failed = true;
        return result;
    }

    loaded.trackName = result.derivedTrackName;
    Track_CopyData(loaded, current);
    current.consumedBytes = loaded.consumedBytes;

    const auto validateTable = [&](std::string_view root, const std::vector<std::string>& files) {
        for (const std::string& file : files) {
            std::string relative(root);
            relative += file;
            if (!resources.Exists(relative)) {
                result.missingFiles.push_back(file);
                missingList += file;
                missingList.push_back('|');
            }
        }
    };
    {
        const std::uint16_t count = Track_FieldFileCount(current);
        std::vector<std::string> declaredFields(current.fieldFiles.begin(),
            current.fieldFiles.begin() + std::min<std::size_t>(count, current.fieldFiles.size()));
        validateTable("TRKDATA/FIELDS/", declaredFields);
    }
    validateTable("TRKDATA/DYNAMICS/", current.dynamicFiles);

    if (missingList.empty()) {
        result.failed = false;
        return result;
    }

    std::string rollbackMissing;
    const TrackReplaceResult rollback =
        Track_ReplaceData(current, live, runtime, backup, resources, rollbackMissing);
    result.rolledBack = true;
    if (rollback.failed ||
        (rollback.diagnostics & TrackReplace_Rollback) != 0)
        result.diagnostics |= TrackLoad_BadRollback;

    result.failed = true;
    return result;
}

}
