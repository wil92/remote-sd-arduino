#pragma once

class ResetButton {
    bool handled_ = false;

public:
    template <typename Clear, typename Restart>
    void check(bool pressed, Clear clear, Restart restart) {
        if (!pressed) {
            handled_ = false;
        } else if (!handled_) {
            handled_ = true;
            if (clear()) {
                restart();
            }
        }
    }
};
