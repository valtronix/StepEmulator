#ifndef ENCODER_H
#define ENCODER_H

#include <Button.h>

class Encoder {
private:
    unsigned char pinA, pinB;
    volatile unsigned long rotatedAt;
    volatile int rot;
    void onEncoderTurned();
public:
    Encoder(unsigned char pin_a, unsigned char pin_b);
    ~Encoder();
    int read();
    void clear();
};

#endif
