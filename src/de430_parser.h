//
// Created by Dmitry Popov on 18.05.2025.
//

#ifndef DE430_DOCKER_H
#define DE430_DOCKER_H

// Error codes
#define DE430_ERROR_NONE 0
#define DE430_ERROR_COMMAND_FAILED -1
#define DE430_ERROR_MEMORY_ALLOCATION -2
#define DE430_ERROR_PARSE_FAILED -3
#define DE430_ERROR_INVALID_CONFIG -4

#define DE430_ERROR_FILE_IO -5
#define DE430_ERROR_JSON_PARSE -6
#define DE430_ERROR_INVALID_CONFIG -7

// Buffer sizes
#define COMMAND_BUFFER_SIZE 4096
#define LINE_BUFFER_SIZE 2048
#define INITIAL_RESULTS_SIZE 1000

/**
 * Data structure representing an astronomical body's ephemeris data
 */
typedef struct {
    double jd;                  // Julian date
    double position[3];         // X, Y, Z position (AU)
    double ra_dec[2];           // RA, Dec (radians)
    double magnitude;           // V-band magnitude
    double phase;               // Phase
    double angular_size;        // Angular size
    double physical_size;       // Physical size
    double albedo;              // Albedo
    double sun_dist;            // Distance from Sun
    double earth_dist;          // Distance from Earth
    double sun_ang_dist;        // Angular distance from Sun
    double theta_edo;           // Elongation parameter
    double ecliptic[3];         // eclLng, eclDist, eclLat
    char constellation[32];     // Constellation name
} DE430EphemerisPoint;

/**
 * Collection of ephemeris data points
 */
typedef struct {
    DE430EphemerisPoint *points;  // Array of data points
    int count;                    // Number of data points in the array
    char object_name[64];         // Name of the astronomical object
} DE430EphemerisData;

/**
 * Configuration for the DE430 request
 */
typedef struct {
    double jd_min;              // Start Julian date
    double jd_max;              // End Julian date
    double jd_step;             // Step size in days
    double *jd_list;            // Explicit list of Julian dates (optional)
    int jd_list_count;          // Number of dates in jd_list
    double latitude;            // Observer latitude (degrees)
    double longitude;           // Observer longitude (degrees)
    int enable_topocentric;     // Whether to enable topocentric correction
    double epoch;               // Epoch for coordinates (default: J2000)
    char objects[256];          // Comma-separated list of objects
    int output_format;          // Output format (-1 to 3)
    int use_orbital_elements;   // Whether to use orbital elements
    int output_constellations;  // Whether to include constellations
} DE430Config;

int de430_save_to_json(const DE430EphemerisData *data, int count, const char *filename);
int de430_load_from_json(const char *filename, DE430EphemerisData **result, int *count);

/**
 * Save ephemeris data to a CSV file
 *
 * @param data Array of ephemeris data to save
 * @param count Number of objects in the array
 * @param filename Name of the file to save to
 * @return 0 on success, error code on failure
 */
int de430_save_to_csv(const DE430EphemerisData *data, int count, const char *filename);

/**
 * Load ephemeris data from a CSV file
 *
 * @param filename Name of the file to load from
 * @param result Pointer to store the resulting data (must be freed with de430_free_data)
 * @param count Number of objects loaded
 * @return 0 on success, error code on failure
 */
int de430_load_from_csv(const char *filename, DE430EphemerisData **result, int *count);

/**
 * Save ephemeris data to a binary file
 *
 * @param data Array of ephemeris data to save
 * @param count Number of objects in the array
 * @param filename Name of the file to save to
 * @return 0 on success, error code on failure
 */
int de430_save_to_binary(const DE430EphemerisData *data, int count, const char *filename);

/**
 * Load ephemeris data from a binary file
 *
 * @param filename Name of the file to load from
 * @param result Pointer to store the resulting data (must be freed with de430_free_data)
 * @param count Number of objects loaded
 * @return 0 on success, error code on failure
 */
int de430_load_from_binary(const char *filename, DE430EphemerisData **result, int *count);
/**
 * Initialize the DE430 configuration with default values
 *
 * @param config Pointer to configuration structure to initialize
 */

void de430_init_config(DE430Config *config);

/**
 * Request ephemeris data from the Docker container
 *
 * @param config Configuration for the request
 * @param result Pointer to store the resulting data (must be freed with de430_free_data)
 * @param count Number of objects returned
 * @return 0 on success, error code on failure
 */
int de430_get_ephemeris(const DE430Config *config, DE430EphemerisData **result, int *count);

/**
 * Free memory allocated for ephemeris data
 *
 * @param data Array of ephemeris data to free
 * @param count Number of objects in the array
 */
void de430_free_data(DE430EphemerisData *data, int count);

/**
 * Get error message for a given error code
 *
 * @param error_code Error code returned by de430_get_ephemeris
 * @return String describing the error
 */
const char* de430_get_error(int error_code);

#endif //DE430_DOCKER_H
