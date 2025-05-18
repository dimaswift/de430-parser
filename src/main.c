#include <stdio.h>
#include <string.h>
#include "de430_parser.h"
#include "cJSON.h"

void print_object_summary(const DE430EphemerisData *data) {
    printf("Object: %s\n", data->object_name);
    printf("Number of data points: %d\n", data->count);

    if (data->count > 0) {
        // Print example of first data point
        DE430EphemerisPoint first = data->points[0];
        printf("  First point (JD %.1f):\n", first.jd);
        printf("    Position (XYZ): %.6f, %.6f, %.6f\n",
               first.position[0], first.position[1], first.position[2]);
        printf("    RA/Dec: %.6f, %.6f\n", first.ra_dec[0], first.ra_dec[1]);
        printf("    Magnitude: %.3f\n", first.magnitude);
        printf("    Distance from Earth: %.6f AU\n", first.earth_dist);

        if (strlen(first.constellation) > 0) {
            printf("    Constellation: %s\n", first.constellation);
        }
    }
}

int main() {
    // Initialize configuration
    DE430Config config;
    de430_init_config(&config);

    // Configure ephemeris request
    config.jd_min = 2451544.5;       // J2000.0
    config.jd_max = 2451554.5;       // J2000.0 + 10 days
    config.jd_step = 1.0;            // 1-day steps
    strcpy(config.objects, "jupiter,mars,saturn");
    config.output_format = 3;        // Full extended format
    config.output_constellations = 1;

    // Get ephemeris data
    DE430EphemerisData *data = NULL;
    int object_count = 0;

    int status = de430_get_ephemeris(&config, &data, &object_count);
    if (status != 0) {
        printf("Error: %s\n", de430_get_error(status));
        return 1;
    }

    printf("Received data for %d objects\n", object_count);

    // Save in different formats
    status = de430_save_to_json(data, object_count, "ephemeris.json");
    if (status == 0) {
        printf("Saved to JSON successfully\n");
    }

    status = de430_save_to_csv(data, object_count, "ephemeris.csv");
    if (status == 0) {
        printf("Saved to CSV successfully\n");
    }

    status = de430_save_to_binary(data, object_count, "ephemeris.bin");
    if (status == 0) {
        printf("Saved to binary successfully\n");
    }

    // Free the original data
    de430_free_data(data, object_count);
    data = NULL;

    // Test loading from each format
    printf("\nLoading from JSON...\n");
    status = de430_load_from_json("ephemeris.json", &data, &object_count);
    if (status == 0) {
        printf("Loaded %d objects from JSON\n", object_count);
        de430_free_data(data, object_count);
        data = NULL;
    }

    printf("\nLoading from CSV...\n");
    status = de430_load_from_csv("ephemeris.csv", &data, &object_count);
    if (status == 0) {
        printf("Loaded %d objects from CSV\n", object_count);
        de430_free_data(data, object_count);
        data = NULL;
    }

    printf("\nLoading from binary...\n");
    status = de430_load_from_binary("ephemeris.bin", &data, &object_count);
    if (status == 0) {
        printf("Loaded %d objects from binary\n", object_count);
        de430_free_data(data, object_count);
        data = NULL;
    }

    return 0;
}