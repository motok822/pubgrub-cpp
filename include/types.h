#pragma once
#include "ranges.h"
#include "incompatibility.h"
#include "arena.h"

// DPから必要な型を一箇所にまとめるためのTraitsクラス
template <class DP>
struct PubGrubTypes {
    using P = typename DP::P;
    using V = typename DP::V;
    using M = typename DP::M;
    using VS = Ranges<V>;
    using Incomp = Incompatibility<P, V, M>;
    using IncompId = Id<Incomp>;
    using Priority = typename DP::Priority;
    using DependencyConstraints = std::map<P, VS>;
};
