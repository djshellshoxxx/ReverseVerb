#pragma once

#include <array>

namespace rv
{
// One extra, freely-placeable node inserted between an envelope's fixed
// start (position 0) and end (position 1) knobs. pos is strictly inside
// (0, 1); value's range depends on what the envelope drives (0..1 gain for
// Volume, -1..1 for Pan).
struct EnvelopePoint
{
    float pos = 0.5f;
    float value = 0.0f;
};

// Only the interior nodes are stored here - the start/end values and the
// curve tension keep coming from the existing Start/End/Tension knobs, so an
// envelope with no interior points behaves exactly like the original 2-point
// control. Mirrors GatePattern: fixed-capacity, POD, easy to serialise/undo.
struct Envelope
{
    static constexpr int maxInteriorPoints = 7;
    std::array<EnvelopePoint, maxInteriorPoints> interior {};
    int numInterior = 0;
};

[[nodiscard]] Envelope sanitiseEnvelope (const Envelope&, float minValue, float maxValue) noexcept;

// Evaluates the envelope at position t (0..1), given the live start/end
// values (from the Start/End knobs) and the shared curve tension (from the
// Tension knob). With no interior points this reduces exactly to
// start + (end - start) * tensionCurve(t, tension).
[[nodiscard]] float envelopeValueAt (const Envelope&, float t, float startValue, float endValue, float tension) noexcept;

// Inserts a new interior point (or, if one already sits within `tolerance`
// of pos, replaces it), keeping interior points sorted by position. Returns
// the point unchanged if the envelope is already at capacity.
[[nodiscard]] Envelope withInteriorPoint (const Envelope&, float pos, float value,
                                           float minValue, float maxValue, float tolerance = 0.02f) noexcept;

// Removes the interior point closest to pos if it's within `tolerance`.
[[nodiscard]] Envelope withoutNearestInteriorPoint (const Envelope&, float pos, float tolerance = 0.035f) noexcept;

// Index of the interior point closest to (pos, value) in normalised screen-ish
// distance, or -1 if none is within `tolerance`. valueSpan normalises the
// value axis (e.g. 2.0f for a -1..1 pan range) so distances are comparable.
[[nodiscard]] int nearestInteriorPoint (const Envelope&, float pos, float value, float valueSpan, float tolerance = 0.05f) noexcept;
}
