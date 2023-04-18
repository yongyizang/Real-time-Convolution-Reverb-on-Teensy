#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Structure to store WAV file header
typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2ID[4];
    uint32_t subchunk2Size;
} WAVHeader;

float convertToFloat(int16_t sample) {
    return (float)sample / 32768.0f;
}

int main() {
    // Open the WAV file
    FILE *file = fopen("focusrite-ir.wav", "rb");
    if (!file) {
        printf("Error: Could not open the WAV file.\n");
        return 1;
    }

    // Read the WAV header
    WAVHeader header;
    fread(&header, sizeof(WAVHeader), 1, file);

    // Validate the WAV file format (mono, 16-bit)
    if (strncmp(header.chunkID, "RIFF", 4) != 0 ||
        strncmp(header.format, "WAVE", 4) != 0 ||
        strncmp(header.subchunk1ID, "fmt ", 4) != 0 ||
        strncmp(header.subchunk2ID, "data", 4) != 0 ||
        header.audioFormat != 1 ||
        header.numChannels != 1 ||
        header.bitsPerSample != 16) {
        printf("Error: Unsupported WAV file format.\n");
        fclose(file);
        return 1;
    }

    // Calculate the number of samples
    int numSamples = header.subchunk2Size / (header.numChannels * (header.bitsPerSample / 8));

    // Allocate memory for the impulse response array
    int16_t *ir = (int16_t *)malloc(numSamples * sizeof(int16_t));
    if (!ir) {
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        return 1;
    }

    // Read the impulse response data from the WAV file
    fread(ir, sizeof(int16_t), numSamples, file);
    fclose(file);

    // Create and open the header file for writing
    FILE *headerFile = fopen("impulse_response.h", "w");
    if (!headerFile) {
        printf("Error: Could not create the header file.\n");
        free(ir);
        return 1;
    }

    // Write the impulse response data to the header file
    fprintf(headerFile, "#ifndef IMPULSE_RESPONSE_H\n");
    fprintf(headerFile, "#define IMPULSE_RESPONSE_H\n\n");
    fprintf(headerFile, "#include <stdint.h>\n\n");
    
    // Write the size.
    fprintf(headerFile, "#define IR_SIZE %d\n\n", numSamples);
    fprintf(headerFile, "const float ir[%d] = {\n", numSamples);

    for (int i = 0; i < numSamples; i++) {
        // directly write float value
        fprintf(headerFile, "%.8f", convertToFloat(ir[i]));
        if (i < numSamples - 1) {
            fprintf(headerFile, ",\n");
        } else {
            fprintf(headerFile, "\n};\n\n");
        }
    }

    fprintf(headerFile, "#endif // IMPULSE_RESPONSE_H\n");

    // Close the header file and free the memory
    fclose(headerFile);
    free(ir);

    return 0;
}