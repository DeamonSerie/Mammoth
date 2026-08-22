// Mammoth unit tests: custom brush system + brush engine regression.
//
// Headless test runner built by `make test`. Uses no external framework,
// matching the project's zero-dependency conventions.

#include "../src/drawing/CurveTypes.hpp"
#include "../src/drawing/CustomBrushConfig.hpp"
#include "../src/drawing/CustomBrushGeometry.hpp"
#include "../src/drawing/Brush.hpp"
#include "../src/drawing/BrushEngine.hpp"
#include "../src/document/Layer.hpp"

#include <cmath>
#include <cstdio>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { \
        g_failures++; \
        std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    g_checks++; \
    if (!((a) == (b))) { \
        g_failures++; \
        std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
    } \
} while (0)

#define CHECK_NEAR(a, b) do { \
    g_checks++; \
    if (std::fabs((float)(a) - (float)(b)) > 1e-4f) { \
        g_failures++; \
        std::printf("FAIL %s:%d: %s (~%f) near %s (~%f)\n", \
                    __FILE__, __LINE__, #a, (double)(a), #b, (double)(b)); \
    } \
} while (0)

static bool covered(const Layer& l, int x, int y) {
    return l.getPixel(x, y).a > 0;
}

static int coveredCount(const Layer& l) {
    int n = 0;
    for (int y = 0; y < l.height(); y++)
        for (int x = 0; x < l.width(); x++)
            if (covered(l, x, y)) n++;
    return n;
}

// ---------------------------------------------------------------------------
// Curve combination rules
// ---------------------------------------------------------------------------

static void testCurveCombination() {
    CHECK(combineCurves(CurveType::Triangle, CurveType::Triangle) == CurveType::Square);
    CHECK(combineCurves(CurveType::Square, CurveType::Square) == CurveType::Circle);
    CHECK(combineCurves(CurveType::Circle, CurveType::Circle) == CurveType::Square);
    CHECK(combineCurves(CurveType::Triangle, CurveType::Square) == CurveType::Triangle);
    CHECK(combineCurves(CurveType::Triangle, CurveType::Circle) == CurveType::Triangle);
    CHECK(combineCurves(CurveType::Circle, CurveType::Square) == CurveType::Circle);

    // Commutativity in both operand orders.
    for (int a = 0; a < curveTypeCount(); a++) {
        for (int b = 0; b < curveTypeCount(); b++) {
            CHECK(combineCurves((CurveType)a, (CurveType)b) ==
                  combineCurves((CurveType)b, (CurveType)a));
        }
    }

    // Determinism: same inputs always produce the same output.
    for (int run = 0; run < 5; run++) {
        CHECK(combineCurves(CurveType::Triangle, CurveType::Square) == CurveType::Triangle);
        CHECK(combineCurves(CurveType::Square, CurveType::Circle) == CurveType::Circle);
    }
}

// ---------------------------------------------------------------------------
// Primary curves
// ---------------------------------------------------------------------------

static void testPrimaryCurves() {
    CustomBrushConfig cfg;
    CHECK(cfg.validate());

    // Four primary selections are stored and default to valid states.
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        CHECK((unsigned)cfg.primary[i] < (unsigned)curveTypeCount());
    }

    // Each primary supports Circle, Square and Triangle.
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        cfg.primary[i] = CurveType::Circle;
        CHECK(cfg.validate() && cfg.primary[i] == CurveType::Circle);
        cfg.primary[i] = CurveType::Square;
        CHECK(cfg.validate() && cfg.primary[i] == CurveType::Square);
        cfg.primary[i] = CurveType::Triangle;
        CHECK(cfg.validate() && cfg.primary[i] == CurveType::Triangle);
    }

    // Changing a primary updates the automatically determined secondary
    // curve it controls (slot 2 is controlled by primary piece 1) and the
    // resolved geometry of that piece's slice.
    CustomBrushConfig c2;
    c2.primary[1] = CurveType::Square;
    CHECK(c2.effectiveSecondary(2) == c2.autoSecondary(2));
    CHECK(c2.autoSecondary(2) == combineCurves(CurveType::Square, CurveType::Square));
    c2.primary[1] = CurveType::Triangle;
    CHECK(c2.autoSecondary(2) == combineCurves(CurveType::Triangle, CurveType::Triangle));

    // Changing a primary changes the resolved combined geometry.
    // (Square and Circle primaries derive different automatic values.)
    CustomBrushConfig a, b;
    a.primary[1] = CurveType::Square;
    b.primary[1] = CurveType::Circle;
    CHECK(a.autoSecondary(2) != b.autoSecondary(2));
    CHECK(a.resolve() != b.resolve());
}

// ---------------------------------------------------------------------------
// Secondary subdivision
// ---------------------------------------------------------------------------

static void testSecondaryDivision() {
    CustomBrushConfig cfg;

    // Four primary pieces produce eight secondary sections.
    CHECK(CUSTOM_PRIMARY_COUNT == 4);
    CHECK(CUSTOM_SECONDARY_COUNT == 8);
    for (int s = 0; s < CUSTOM_SECONDARY_COUNT; s++) {
        CHECK(CustomBrushConfig::controllingPrimary(s) == s / 2);
        cfg.setSecondaryParam(s, 0.5f);
        CHECK_NEAR(cfg.secondaryParam[s], 0.5f);
    }

    // Every third slot in the sequence is automatic (slots 2 and 5).
    CHECK(CustomBrushConfig::isAutoSecondary(2));
    CHECK(CustomBrushConfig::isAutoSecondary(5));
    CHECK(!CustomBrushConfig::isAutoSecondary(0));
    CHECK(!CustomBrushConfig::isAutoSecondary(7));

    // Automatically determined curves cannot be manually overridden.
    CustomBrushConfig locked;
    CHECK(!locked.setSecondaryCurve(2, CurveType::Triangle));
    CHECK(!locked.setSecondaryCurve(5, CurveType::Square));
    // Values remain at their defaults (band centres 5/6 and 1/6).
    CHECK(locked.secondaryParam[2] != 0.5f);
    CHECK(locked.secondaryParam[5] != 0.5f);

    // User-controlled slots accept all three curve types.
    for (int s : {0, 1, 3, 4, 6, 7}) {
        CHECK(cfg.setSecondaryCurve(s, CurveType::Circle));
        CHECK(cfg.effectiveSecondary(s) == CurveType::Circle);
        CHECK(cfg.setSecondaryCurve(s, CurveType::Square));
        CHECK(cfg.effectiveSecondary(s) == CurveType::Square);
        CHECK(cfg.setSecondaryCurve(s, CurveType::Triangle));
        CHECK(cfg.effectiveSecondary(s) == CurveType::Triangle);
    }

    // Changing the controlling primary recalculates the dependent auto curve.
    // (Square and Circle primaries derive different automatic values.)
    // Slot 4 is set to Circle so the slice's combined type differs between
    // the two cases.
    CustomBrushConfig dep;
    dep.setSecondaryCurve(4, CurveType::Circle);
    dep.primary[2] = CurveType::Square;        // controls slots 4 and 5
    CHECK(dep.autoSecondary(5) ==
          combineCurves(CurveType::Square, CurveType::Square));
    ResolvedCustomBrush before = dep.resolve();
    dep.primary[2] = CurveType::Circle;
    CHECK(dep.autoSecondary(5) ==
          combineCurves(CurveType::Circle, CurveType::Circle));
    CHECK(dep.resolve() != before);

    // Out-of-range slot indices are rejected without effect.
    CHECK(!cfg.setSecondaryCurve(-1, CurveType::Circle));
    CHECK(!cfg.setSecondaryCurve(CUSTOM_SECONDARY_COUNT, CurveType::Circle));
}

// ---------------------------------------------------------------------------
// Combined-curve dimensions
// ---------------------------------------------------------------------------

static void testDimensions() {
    CustomBrushConfig cfg;

    // Each resulting curve has an independent height setting.
    cfg.setHeight(0, 0.5f);
    cfg.setHeight(1, 1.5f);
    CHECK_NEAR(cfg.height[0], 0.5f);
    CHECK_NEAR(cfg.height[1], 1.5f);
    CHECK(cfg.resolve().curves[0].height != cfg.resolve().curves[1].height);

    // Each resulting curve has an independent width setting.
    cfg.setWidth(0, 0.5f);
    cfg.setWidth(3, 1.25f);
    CHECK_NEAR(cfg.width[0], 0.5f);
    CHECK_NEAR(cfg.width[3], 1.25f);
    CHECK(cfg.resolve().curves[0].width != cfg.resolve().curves[3].width);

    // Clamping of out-of-range values.
    cfg.setHeight(0, -5.0f);
    CHECK_NEAR(cfg.height[0], CUSTOM_MIN_DIM);
    cfg.setHeight(0, 99.0f);
    CHECK_NEAR(cfg.height[0], CUSTOM_MAX_HEIGHT);
    cfg.setWidth(0, -5.0f);
    CHECK_NEAR(cfg.width[0], CUSTOM_MIN_DIM);
    cfg.setWidth(0, 99.0f);
    CHECK_NEAR(cfg.width[0], CUSTOM_MAX_WIDTH);
    cfg.setSecondaryParam(0, -1.0f);
    CHECK_NEAR(cfg.secondaryParam[0], 0.0f);
    cfg.setSecondaryParam(0, 2.0f);
    CHECK_NEAR(cfg.secondaryParam[0], 1.0f);

    // Height changes affect generated geometry.
    CustomBrushConfig hLow, hHigh;
    hLow.setHeight(0, CUSTOM_MIN_DIM);
    hHigh.setHeight(0, CUSTOM_MAX_HEIGHT);
    int lowPix = CustomBrushGeometry::coveragePixelCount(hLow.resolve(), 16.0f);
    int highPix = CustomBrushGeometry::coveragePixelCount(hHigh.resolve(), 16.0f);
    CHECK(highPix > lowPix);

    // Width changes affect generated geometry.
    CustomBrushConfig wNarrow;
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) wNarrow.setWidth(i, CUSTOM_MIN_DIM);
    CustomBrushConfig wWide;
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) wWide.setWidth(i, CUSTOM_MAX_WIDTH);
    int narrow = CustomBrushGeometry::coveragePixelCount(wNarrow.resolve(), 16.0f);
    int wide = CustomBrushGeometry::coveragePixelCount(wWide.resolve(), 16.0f);
    CHECK(wide > narrow);

    // Height/width only affect their own curve slot.
    CustomBrushConfig base;
    ResolvedCustomBrush rb = base.resolve();
    CustomBrushConfig mod = base;
    mod.setHeight(4, 1.75f);
    mod.setWidth(4, 0.5f);
    ResolvedCustomBrush rm = mod.resolve();
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        if (i == 4) continue;
        CHECK(rm.curves[i] == rb.curves[i]);
    }
    CHECK_NEAR(rm.curves[4].height, 1.75f);
    CHECK_NEAR(rm.curves[4].width, 0.5f);
}

// ---------------------------------------------------------------------------
// Curve combination through resolution
// ---------------------------------------------------------------------------

static void testCombinedCurveTypes() {
    // Combination happens only between Division-2 curves, paired per slice:
    // the two sections of each primary piece combine with each other, and
    // both resulting curves of the slice take the combined type.
    CustomBrushConfig cfg;
    cfg.setSecondaryCurve(0, CurveType::Triangle);
    cfg.setSecondaryCurve(1, CurveType::Triangle);
    CHECK(cfg.resolve().curves[0].type == CurveType::Square);
    CHECK(cfg.resolve().curves[1].type == CurveType::Square);

    // Both curves of a slice always share the combined type.
    // (Pieces 0 and 3 own only user-controlled sections; pieces 1 and 2
    // contain automatic slots driven by their controlling primary.)
    for (int p : {0, 3}) {
        CustomBrushConfig c;
        c.setSecondaryCurve(p * 2, CurveType::Square);
        c.setSecondaryCurve(p * 2 + 1, CurveType::Triangle);
        ResolvedCustomBrush r = c.resolve();
        CurveType expected = combineCurves(CurveType::Square, CurveType::Triangle);
        CHECK(r.curves[p * 2].type == expected);
        CHECK(r.curves[p * 2 + 1].type == expected);
    }

    // A slice containing an automatic section combines with it too.
    CustomBrushConfig autoSlice;
    autoSlice.primary[1] = CurveType::Circle;   // slot 2 becomes Square
    autoSlice.setSecondaryCurve(3, CurveType::Triangle);
    CHECK(autoSlice.resolve().curves[2].type ==
          combineCurves(CurveType::Square, CurveType::Triangle));
    CHECK(autoSlice.resolve().curves[3].type == CurveType::Triangle);

    // All six combination rules surface in resolved slices.
    // Slice 0 (slots 0,1) checks forward order; slice 3 (slots 6,7) checks
    // the flipped operand order.
    struct Rule { CurveType x, y, result; };
    const Rule rules[] = {
        { CurveType::Triangle, CurveType::Triangle, CurveType::Square },
        { CurveType::Square,   CurveType::Square,   CurveType::Circle },
        { CurveType::Circle,   CurveType::Circle,   CurveType::Square },
        { CurveType::Triangle, CurveType::Square,   CurveType::Triangle },
        { CurveType::Triangle, CurveType::Circle,   CurveType::Triangle },
        { CurveType::Circle,   CurveType::Square,   CurveType::Circle },
    };
    for (const Rule& rule : rules) {
        CustomBrushConfig c;
        c.setSecondaryCurve(0, rule.x);
        c.setSecondaryCurve(1, rule.y);
        CHECK(c.resolve().curves[0].type == rule.result);

        CustomBrushConfig flipped;
        flipped.setSecondaryCurve(6, rule.y);
        flipped.setSecondaryCurve(7, rule.x);
        CHECK(flipped.resolve().curves[7].type == rule.result);
    }

    // Slices are isolated: changing one slice never affects another.
    CustomBrushConfig base;
    ResolvedCustomBrush rb = base.resolve();
    CustomBrushConfig changed = base;
    changed.setSecondaryCurve(1, CurveType::Circle); // slice 0 only
    changed.setSecondaryCurve(7, CurveType::Circle); // slice 3 only
    ResolvedCustomBrush rc = changed.resolve();
    CHECK(rc.curves[0].type != rb.curves[0].type ||
          rc.curves[1].type != rb.curves[1].type);
    CHECK(rc.curves[2].type == rb.curves[2].type);
    CHECK(rc.curves[3].type == rb.curves[3].type);
    CHECK(rc.curves[4].type == rb.curves[4].type);
    CHECK(rc.curves[5].type == rb.curves[5].type);
}

// ---------------------------------------------------------------------------
// Brush generation
// ---------------------------------------------------------------------------

static void testBrushGeneration() {
    // A complete valid configuration generates brush geometry.
    CustomBrushConfig cfg;
    CHECK(cfg.validate());
    ResolvedCustomBrush resolved = cfg.resolve();
    CHECK(resolved.isValid());
    int pixels = CustomBrushGeometry::coveragePixelCount(resolved, 20.0f);
    CHECK(pixels > 100);

    // Geometry is deterministic across repeated generations.
    CHECK(CustomBrushGeometry::coveragePixelCount(resolved, 20.0f) == pixels);

    // Changing a primary curve changes expected geometry.
    // (Square and Circle primaries derive different automatic values;
    // primary piece 1 controls the automatic slot 2.)
    CustomBrushConfig pA, pB;
    pA.primary[1] = CurveType::Square;
    pB.primary[1] = CurveType::Circle;
    CHECK(CustomBrushGeometry::coveragePixelCount(pA.resolve(), 20.0f) !=
          CustomBrushGeometry::coveragePixelCount(pB.resolve(), 20.0f));

    // Changing a secondary configuration changes expected geometry.
    CustomBrushConfig sA, sB;
    sB.setSecondaryCurve(1, sA.effectiveSecondary(1) == CurveType::Circle
                                 ? CurveType::Triangle : CurveType::Circle);
    CHECK(CustomBrushGeometry::coveragePixelCount(sA.resolve(), 20.0f) !=
          CustomBrushGeometry::coveragePixelCount(sB.resolve(), 20.0f));

    // Segment profile shapes are ordered: triangle <= square <= circle area.
    float sum[3] = {0, 0, 0};
    const int N = 64;
    for (int i = 0; i < N; i++) {
        float t = (i + 0.5f) / N;
        sum[(int)CurveType::Triangle] += CustomBrushGeometry::segmentProfile(CurveType::Triangle, t);
        sum[(int)CurveType::Square] += CustomBrushGeometry::segmentProfile(CurveType::Square, t);
        sum[(int)CurveType::Circle] += CustomBrushGeometry::segmentProfile(CurveType::Circle, t);
    }
    CHECK(sum[(int)CurveType::Triangle] < sum[(int)CurveType::Square]);
    CHECK(sum[(int)CurveType::Square] < sum[(int)CurveType::Circle]);

    // Profile stays within [0,1] everywhere.
    for (int ti = 0; ti < curveTypeCount(); ti++) {
        for (int i = 0; i <= 100; i++) {
            float p = CustomBrushGeometry::segmentProfile(
                (CurveType)ti, i / 100.0f);
            CHECK(p >= 0.0f && p <= 1.0f);
        }
    }

    // Larger radius produces more covered pixels.
    CHECK(CustomBrushGeometry::coveragePixelCount(resolved, 24.0f) >
          CustomBrushGeometry::coveragePixelCount(resolved, 12.0f));
}

// ---------------------------------------------------------------------------
// Stamping into a layer / painting integration
// ---------------------------------------------------------------------------

static void testStamping() {
    Layer layer(64, 64);
    CustomBrushConfig cfg;
    Color red(255, 30, 30, 255);

    CustomBrushGeometry::stamp(layer, 32.0f, 32.0f, 12.0f, cfg.resolve(), red);
    int painted = coveredCount(layer);
    CHECK(painted > 50);

    // Painted pixels carry the brush color.
    bool foundRed = false;
    for (int y = 0; y < 64 && !foundRed; y++)
        for (int x = 0; x < 64 && !foundRed; x++)
            if (covered(layer, x, y)) {
                Color c = layer.getPixel(x, y);
                foundRed = (c.r == 255 && c.g == 30 && c.b == 30);
            }
    CHECK(foundRed);

    // Invalid configurations paint nothing.
    Layer empty(64, 64);
    ResolvedCustomBrush bad;
    bad.curves[0].height = 99.0f; // invalid -> isValid() false
    CHECK(!bad.isValid());
    CustomBrushGeometry::stamp(empty, 32.0f, 32.0f, 12.0f, bad, red);
    CHECK(coveredCount(empty) == 0);

    // Opacity is respected through BrushEngine.
    Brush opaque;
    opaque.setType(BrushType::Custom);
    opaque.setSize(16.0f);
    opaque.setColor(Color(0, 0, 255, 255));
    opaque.setOpacity(1.0f);

    Brush translucent = opaque;
    translucent.setOpacity(0.25f);

    Layer lo(48, 48), lt(48, 48);
    BrushEngine eng;
    eng.applyStamp(lo, 24.0f, 24.0f, opaque);
    eng.applyStamp(lt, 24.0f, 24.0f, translucent);

    int oAlpha = 0, tAlpha = 0;
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 48; x++) {
            oAlpha += lo.getPixel(x, y).a;
            tAlpha += lt.getPixel(x, y).a;
        }
    CHECK(oAlpha > tAlpha * 2);

    // Coverage exists for the custom brush through the engine pipeline.
    CHECK(coveredCount(lo) > 30);
}

// ---------------------------------------------------------------------------
// Validation & serialization
// ---------------------------------------------------------------------------

static void testValidationAndSerialization() {
    CustomBrushConfig cfg;
    CHECK(cfg.validate());
    CHECK(cfg.serialize().rfind("CB1|", 0) == 0);

    // Round trip preserves all state.
    CustomBrushConfig mod;
    mod.primary[0] = CurveType::Triangle;
    mod.primary[3] = CurveType::Square;
    mod.setSecondaryCurve(0, CurveType::Triangle);
    mod.setSecondaryParam(1, 0.73f);
    mod.setHeight(2, 1.4f);
    mod.setWidth(3, 0.8f);
    std::string s = mod.serialize();
    CustomBrushConfig back = CustomBrushConfig::deserialize(s);
    CHECK(back == mod);
    CHECK(back.validate());

    // Garbage input yields defaults rather than an invalid model.
    CustomBrushConfig junk = CustomBrushConfig::deserialize("garbage");
    CHECK(junk.validate());
    CustomBrushConfig empty = CustomBrushConfig::deserialize("");
    CHECK(empty.validate());
}

// ---------------------------------------------------------------------------
// Regression: existing brush behaviour
// ---------------------------------------------------------------------------

static void testExistingBrushRegression() {
    BrushEngine eng;

    // HardRound stamps a filled disc.
    Brush hard;
    hard.setType(BrushType::HardRound);
    hard.setSize(10.0f);
    hard.setColor(Color(255, 0, 0, 255));
    Layer l1(40, 40);
    eng.applyStamp(l1, 20.0f, 20.0f, hard);
    CHECK(covered(l1, 20, 20));
    CHECK(!covered(l1, 20, 26));   // outside radius
    CHECK(covered(l1, 20, 24));    // inside radius

    // SoftRound has softer falloff inside the edge.
    Brush soft;
    soft.setType(BrushType::SoftRound);
    soft.setSize(10.0f);
    soft.setHardness(0.5f);
    soft.setColor(Color(0, 255, 0, 255));
    Layer l2(40, 40);
    eng.applyStamp(l2, 20.0f, 20.0f, soft);
    CHECK(l2.getPixel(20, 20).a == 255);
    CHECK(l2.getPixel(20, 24).a < 255); // partial coverage near edge
    CHECK(l2.getPixel(20, 24).a > 0);

    // Stroke interpolation still produces points along the segment.
    Brush b;
    b.setSize(8.0f);
    b.setSpacing(0.25f);
    auto pts = b.interpolatePoints({0, 0}, {10, 0});
    CHECK(pts.size() >= 2);
    CHECK(pts.front().x == 0.0f);
    CHECK(pts.back().x == 10.0f);

    // Existing brush types are untouched by custom-brush additions.
    Brush pencil;
    pencil.setType(BrushType::Pencil);
    Layer l3(32, 32);
    eng.applyStamp(l3, 16.0f, 16.0f, pencil);
    CHECK(covered(l3, 16, 16));

    // Custom enum value did not shift existing ones.
    CHECK((int)BrushType::HardRound == 0);
    CHECK((int)BrushType::Eraser == 4);
}

// ---------------------------------------------------------------------------

int main() {
    testCurveCombination();
    testPrimaryCurves();
    testSecondaryDivision();
    testDimensions();
    testCombinedCurveTypes();
    testBrushGeneration();
    testStamping();
    testValidationAndSerialization();
    testExistingBrushRegression();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
