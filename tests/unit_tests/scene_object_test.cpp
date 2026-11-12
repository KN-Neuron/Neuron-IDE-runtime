#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "data_structures/Context.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"
#include "scene/components/Component.hpp"

namespace {
constexpr double kBlinkFrequencyHz = 1.0;
constexpr double kHalfSecondStep   = 0.5;
constexpr double kStep             = 0.05;

// Counts how often it is updated and drawn, and can veto drawing.
class SpyComponent : public Component {
   public:
    SpyComponent(const std::shared_ptr<SceneObject>& owner, bool allowDrawing)
        : Component(owner), allowDrawing(allowDrawing) {}

    void update(const Context& context) override {
        (void)context;
        ++updateCalls;
    }
    void render(SDL_Renderer* renderer) override {
        (void)renderer;
        ++renderCalls;
    }
    bool allowsDrawing() const override { return allowDrawing; }

    [[nodiscard]] int updates() const { return updateCalls; }
    [[nodiscard]] int renders() const { return renderCalls; }

   private:
    bool allowDrawing;
    int  updateCalls = 0;
    int  renderCalls = 0;
};

// Adds a spy to `object` and returns a pointer to it; the object keeps ownership.
SpyComponent* addSpy(const std::shared_ptr<SceneObject>& object, bool allowDrawing) {
    auto  spy = std::make_unique<SpyComponent>(object, allowDrawing);
    auto* raw = spy.get();
    object->addComponent(std::move(spy));
    return raw;
}
}  // namespace

TEST(SceneObjectTest, RendersComponentsWhenAllAllowDrawing) {
    auto  object = std::make_shared<SceneObject>("Obj");
    auto* first  = addSpy(object, true);
    auto* second = addSpy(object, true);

    object->render(nullptr);

    EXPECT_EQ(first->renders(), 1);
    EXPECT_EQ(second->renders(), 1);
}

// One component saying "do not draw" is enough to skip the whole object, including components that
// would have allowed it.
TEST(SceneObjectTest, SkipsRenderWhenAnyComponentVetoesDrawing) {
    auto  object   = std::make_shared<SceneObject>("Obj");
    auto* allowing = addSpy(object, true);
    auto* vetoing  = addSpy(object, false);

    object->render(nullptr);

    EXPECT_EQ(allowing->renders(), 0);
    EXPECT_EQ(vetoing->renders(), 0);
}

// A veto only affects drawing: the vetoing component and its siblings keep getting update().
TEST(SceneObjectTest, VetoedObjectIsStillUpdated) {
    auto  object  = std::make_shared<SceneObject>("Obj");
    auto* vetoing = addSpy(object, false);

    object->update(Context{kStep, nullptr});

    EXPECT_EQ(vetoing->updates(), 1);
}

// End to end with a real blinker: the object is drawn in the lit half of the cycle and skipped in
// the unlit half, while its own isActive flag is never touched.
TEST(SceneObjectTest, BlinkerHidesObjectOnlyThroughAllowsDrawing) {
    auto object = std::make_shared<SceneObject>("Obj");
    object->addComponent(std::make_unique<BlinkComponent>(object, kBlinkFrequencyHz));
    auto* spy = addSpy(object, true);

    object->update(Context{kStep, nullptr});
    object->render(nullptr);
    EXPECT_EQ(spy->renders(), 1);

    object->update(Context{kHalfSecondStep, nullptr});
    object->render(nullptr);
    EXPECT_EQ(spy->renders(), 1);

    EXPECT_TRUE(object->isActive);
}

// An inactive object is out of the scene: nothing on it is updated or drawn.
TEST(SceneObjectTest, InactiveObjectSkipsUpdateAndRender) {
    auto  object = std::make_shared<SceneObject>("Obj", false);
    auto* spy    = addSpy(object, true);

    object->update(Context{kStep, nullptr});
    object->render(nullptr);

    EXPECT_EQ(spy->updates(), 0);
    EXPECT_EQ(spy->renders(), 0);
}

// Activating the object again brings its components back.
TEST(SceneObjectTest, ReactivatedObjectUpdatesAndRendersAgain) {
    auto  object = std::make_shared<SceneObject>("Obj", false);
    auto* spy    = addSpy(object, true);

    object->isActive = true;
    object->update(Context{kStep, nullptr});
    object->render(nullptr);

    EXPECT_EQ(spy->updates(), 1);
    EXPECT_EQ(spy->renders(), 1);
}
