//
// Created by Dmitry Popov on 18.05.2025.
//

/**
 * Save ephemeris data to a CSV file
 *
 * @param data Array of ephemeris data to save
 * @param count Number of objects in the array
 * @param filename Name of the file to save to
 * @return 0 on success, error code on failure
 */

#include "de430_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int de430_save_to_csv(const DE430EphemerisData *data, int count, const char *filename) {
    if (!data || count <= 0 || !filename) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Open the file
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return DE430_ERROR_FILE_IO;
    }

    // Write the header
    fprintf(fp, "object_name,jd,pos_x,pos_y,pos_z,ra,dec,magnitude,phase,angular_size,");
    fprintf(fp, "physical_size,albedo,sun_dist,earth_dist,sun_ang_dist,theta_edo,");
    fprintf(fp, "ecliptic_lng,ecliptic_dist,ecliptic_lat,constellation\n");

    // Write each data point
    for (int i = 0; i < count; i++) {
        const DE430EphemerisData *obj = &data[i];

        for (int j = 0; j < obj->count; j++) {
            const DE430EphemerisPoint *point = &obj->points[j];

            // Format object name, replacing commas with spaces
            char safe_name[65];
            strncpy(safe_name, obj->object_name, 64);
            safe_name[64] = '\0';

            for (char *p = safe_name; *p; p++) {
                if (*p == ',') *p = ' ';
            }

            // Format constellation, replacing commas with spaces
            char safe_const[33];
            strncpy(safe_const, point->constellation, 32);
            safe_const[32] = '\0';

            for (char *p = safe_const; *p; p++) {
                if (*p == ',') *p = ' ';
            }

            // Write the data point
            fprintf(fp, "%s,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,",
                   safe_name, point->jd,
                   point->position[0], point->position[1], point->position[2],
                   point->ra_dec[0], point->ra_dec[1],
                   point->magnitude, point->phase, point->angular_size);

            fprintf(fp, "%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%s\n",
                   point->physical_size, point->albedo,
                   point->sun_dist, point->earth_dist, point->sun_ang_dist, point->theta_edo,
                   point->ecliptic[0], point->ecliptic[1], point->ecliptic[2],
                   safe_const);
        }
    }

    fclose(fp);
    return DE430_ERROR_NONE;
}

/**
 * Load ephemeris data from a CSV file
 *
 * @param filename Name of the file to load from
 * @param result Pointer to store the resulting data (must be freed with de430_free_data)
 * @param count Number of objects loaded
 * @return 0 on success, error code on failure
 */
int de430_load_from_csv(const char *filename, DE430EphemerisData **result, int *count) {
    if (!filename || !result || !count) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Open the file
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return DE430_ERROR_FILE_IO;
    }

    // Read and skip the header
    char line[LINE_BUFFER_SIZE];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return DE430_ERROR_PARSE_FAILED;
    }

    // First pass: count unique object names and points per object
    struct {
        char name[64];
        int count;
    } *objects = NULL;

    int object_count = 0;
    int total_points = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines
        if (line[0] == '\n' || line[0] == '\0') continue;

        // Parse object name (first field)
        char object_name[64] = {0};
        char *comma = strchr(line, ',');
        if (!comma) continue;

        strncpy(object_name, line, comma - line < 63 ? comma - line : 63);

        // Check if we've seen this object before
        int found = 0;
        for (int i = 0; i < object_count; i++) {
            if (strcmp(objects[i].name, object_name) == 0) {
                objects[i].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            // Add a new object
            objects = realloc(objects, (object_count + 1) * sizeof(*objects));
            if (!objects) {
                fclose(fp);
                return DE430_ERROR_MEMORY_ALLOCATION;
            }

            strcpy(objects[object_count].name, object_name);
            objects[object_count].count = 1;
            object_count++;
        }

        total_points++;
    }

    // Allocate result array
    *result = malloc(object_count * sizeof(DE430EphemerisData));
    if (!*result) {
        free(objects);
        fclose(fp);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Initialize result objects
    for (int i = 0; i < object_count; i++) {
        strcpy((*result)[i].object_name, objects[i].name);
        (*result)[i].count = objects[i].count;
        (*result)[i].points = malloc(objects[i].count * sizeof(DE430EphemerisPoint));

        if (!(*result)[i].points) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            free(objects);
            fclose(fp);
            return DE430_ERROR_MEMORY_ALLOCATION;
        }

        // Reset point counter for second pass
        objects[i].count = 0;
    }

    // Second pass: read the data
    rewind(fp);

    // Skip header again
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines
        if (line[0] == '\n' || line[0] == '\0') continue;

        // Make a copy of the line to tokenize
        char *line_copy = strdup(line);
        if (!line_copy) {
            // Clean up on failure
            for (int i = 0; i < object_count; i++) {
                free((*result)[i].points);
            }
            free(*result);
            *result = NULL;
            free(objects);
            fclose(fp);
            return DE430_ERROR_MEMORY_ALLOCATION;
        }

        // Tokenize the line
        char *token;
        char *saveptr = NULL;
        int field = 0;

        // Get object name
        token = strtok_r(line_copy, ",", &saveptr);
        if (!token) {
            free(line_copy);
            continue;
        }

        char object_name[64];
        strncpy(object_name, token, 63);
        object_name[63] = '\0';

        // Find the object index
        int obj_idx = -1;
        for (int i = 0; i < object_count; i++) {
            if (strcmp((*result)[i].object_name, object_name) == 0) {
                obj_idx = i;
                break;
            }
        }

        if (obj_idx == -1) {
            // This shouldn't happen if the two passes are consistent
            free(line_copy);
            continue;
        }

        // Get the point index for this object
        int point_idx = objects[obj_idx].count++;
        DE430EphemerisPoint *point = &((*result)[obj_idx].points[point_idx]);

        // Clear the point
        memset(point, 0, sizeof(DE430EphemerisPoint));

        // Parse remaining fields
        // JD
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->jd = atof(token);

        // Position
        for (int i = 0; i < 3; i++) {
            token = strtok_r(NULL, ",", &saveptr);
            if (token) point->position[i] = atof(token);
        }

        // RA/Dec
        for (int i = 0; i < 2; i++) {
            token = strtok_r(NULL, ",", &saveptr);
            if (token) point->ra_dec[i] = atof(token);
        }

        // Magnitude
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->magnitude = atof(token);

        // Phase
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->phase = atof(token);

        // Angular size
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->angular_size = atof(token);

        // Physical size
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->physical_size = atof(token);

        // Albedo
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->albedo = atof(token);

        // Sun distance
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->sun_dist = atof(token);

        // Earth distance
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->earth_dist = atof(token);

        // Sun angular distance
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->sun_ang_dist = atof(token);

        // Theta EDO
        token = strtok_r(NULL, ",", &saveptr);
        if (token) point->theta_edo = atof(token);

        // Ecliptic
        for (int i = 0; i < 3; i++) {
            token = strtok_r(NULL, ",", &saveptr);
            if (token) point->ecliptic[i] = atof(token);
        }

        // Constellation (last field)
        token = strtok_r(NULL, "\n", &saveptr);
        if (token) {
            strncpy(point->constellation, token, sizeof(point->constellation) - 1);
            point->constellation[sizeof(point->constellation) - 1] = '\0';
        }

        free(line_copy);
    }

    free(objects);
    fclose(fp);

    *count = object_count;
    return DE430_ERROR_NONE;
}