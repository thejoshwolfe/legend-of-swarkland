#ifndef VISION_TYPES_HPP
#define VISION_TYPES_HPP

struct VisionTypes {
    unsigned normal : 1;
    unsigned ethereal : 1;

    bool any() const {
        return normal || ethereal;
    }
};

static const VisionTypes no_vision = {0, 0};

#endif
