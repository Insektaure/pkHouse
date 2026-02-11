#pragma once

// Supported game types
enum class GameType { ZA, S, V, Sw, Sh, BD, SP, LA };

inline bool isSV(GameType g) { return g == GameType::S || g == GameType::V; }
inline bool isSwSh(GameType g) { return g == GameType::Sw || g == GameType::Sh; }
inline bool isBDSP(GameType g) { return g == GameType::BD || g == GameType::SP; }
