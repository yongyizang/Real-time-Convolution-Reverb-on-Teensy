#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include <string.h>
#include <stdlib.h>

#define buffer_frame_count 200 // how many frames does the buffer hold
#define delay_frame_count 0 // hyper paramater that denots how many frames does playhead delays from writehead.
#define scale_factor 0.1f // hyper parameter that scales the IR. This is to prevent overflow.

class IRConvolver : public AudioStream {
public:
    IRConvolver(size_t size) : AudioStream(1, inputQueueArray) {
        is_conv_setup = false;
        write_head = 0;
        play_head = 0;
        overwrite_head = 0;
        memset(output_buffer, 0, sizeof(output_buffer));
        ir_buffer_count = size / 128;

        ir_buffers = (float**)malloc(ir_buffer_count * sizeof(float*));
        if (!ir_buffers) {
            Serial.println("Failed to allocate memory for IR buffers.");
            return;
        } else {
            memset(ir_buffers, 0, ir_buffer_count * sizeof(float*));
            Serial.println("IR buffers allocated.");
        }
    }

    void setImpulseResponse(const float* ir, size_t size) {
        if (is_conv_setup) {
            Serial.println("IR buffer already initialized.");
            return;
        }
        for (int i = 0; i < ir_buffer_count; ++i) {
            float curr_ir_segment[128];
            for (int j = 0; j < 128; ++j) {
                curr_ir_segment[j] = ir[i * 128 + j] * scale_factor;
            }
            ir_buffers[i] = (float*)malloc(256 * sizeof(float));
            if (!ir_buffers[i]) {
                Serial.println("Failed to allocate memory for IR buffer " + String(i));
                return;
            } else {
                memset(ir_buffers[i], 0, 256 * sizeof(float));
            }
            run_fft(curr_ir_segment, ir_buffers[i]);
        }
        Serial.println("IR buffers set.");
        is_conv_setup = true;
    }

    virtual void update() override {
        if (!is_conv_setup){
            return;
        }

        write_head = (write_head + 1) % buffer_frame_count;
        play_head = (write_head + buffer_frame_count - delay_frame_count) % buffer_frame_count;
        overwrite_head = (play_head + buffer_frame_count - 1) % buffer_frame_count;
        memset(output_buffer + 128 * overwrite_head, 0, 128 * sizeof(float));

        audio_block_t* input = receiveReadOnly();
        if (!input) return;
        audio_block_t* output = allocate();
        if (!output) return;

        if (canPlay()) {
            float output_f[128];
            for (int i = 0; i < 128; ++i) {
                output_f[i] = output_buffer[128 * play_head + i];
                if (output_f[i] > 1.0f) {
                    output_f[i] = 1.0f;
                    Serial.println("overflow at play_head = " + String(play_head));
                } else if (output_f[i] < -1.0f) {
                    output_f[i] = -1.0f;
                    Serial.println("underflow at play_head = " + String(play_head));
                }
            }
            arm_float_to_q15(output_f, output->data, 128);
        } else {
            memset(output->data, 0, 128 * sizeof(int16_t));
        }

        // get input signal in float format
        float input_f[128];
        arm_q15_to_float(input->data, input_f, 128);
        float input_fft[256];
        run_fft(input_f, input_fft);

        for (int i = 0; i < ir_buffer_count; ++i) {
            int local_write_head = (write_head + i) % buffer_frame_count;
            float product[512];
            memset(product, 0, 512 * sizeof(float));
            multiply_fft(input_fft, ir_buffers[i], product);

            float output_ifft[256];
            run_ifft(product, output_ifft);

            if (local_write_head <= buffer_frame_count - 2){
                for (int j = 0; j < 256; ++j) {
                    output_buffer[128 * local_write_head + j] += output_ifft[j];
                }
            } else {
                // write the first 128 samples to the end of the buffer
                for (int j = 0; j < 128; ++j) {
                    output_buffer[128 * local_write_head + j] += output_ifft[j];
                }
                // write the second 128 samples to the beginning of the buffer
                for (int j = 0; j < 128; ++j) {
                    output_buffer[j] += output_ifft[128 + j];
                }
            }
        }
        release(input);
        transmit(output);
        release(output);
    }

    ~DoubleOverlapAddConvolver() {
        for (uint16_t i = 0; i < ir_buffer_count; ++i) {
            free(ir_buffers[i]);
        }
        free(ir_buffers);
    }

private:
    bool canPlay() {
        // this is hard-coded in a terrible way, and takes advantage of the overflow behavior of unsigned ints
        // should be fixed later with a flag
        return play_head < 60000;
    }
    void run_fft(const float* input, float* output) {
        memcpy(output, input, 128 * sizeof(float));
        memset(output + 128, 0, 128 * sizeof(float));
        arm_cfft_f32(&arm_cfft_sR_f32_len128, output, 0, 1);
    }

    void multiply_fft(const float* input1, const float* input2, float* output) {
        arm_cmplx_mult_cmplx_f32(input1, input2, output, 128);
    }

    void run_ifft(const float* input, float* output) {
        memcpy(output, input, 256 * sizeof(float));
        arm_cfft_f32(&arm_cfft_sR_f32_len128, output, 1, 1);
    }
    bool is_conv_setup;
    uint16_t write_head;
    uint16_t play_head;
    uint16_t overwrite_head;
    audio_block_t* inputQueueArray[1];
    float output_buffer[128 * buffer_frame_count];
    float** ir_buffers;
    uint16_t ir_buffer_count;
};