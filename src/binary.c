//
// Created by Dmitry Popov on 18.05.2025.
//
#include "de430_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Binary format header (for versioning and validation)
typedef struct {
    char magic[4];         // "DE43" magic identifier
    uint32_t version;      // Format version (currently 1)
    uint32_t object_count; // Number of objects in the file
    uint32_t reserved;     // Reserved for future use
} DE430BinaryHeader;

// Binary object header (precedes each object's data)
typedef struct {
    uint32_t name_length;  // Length of object name
    uint32_t point_count;  // Number of data points for this object
} DE430BinaryObjectHeader;

// Binary point header (fixed size part of each data point)
typedef struct {
    double jd;
    double position[3];
    double ra_dec[2];
    double magnitude;
    double phase;
    double angular_size;
    double physical_size;
    double albedo;
    double sun_dist;
    double earth_dist;
    double sun_ang_dist;
    double theta_edo;
    double ecliptic[3];
    uint32_t constellation_length;
} DE430BinaryPointHeader;

/**
 * Save ephemeris data to a binary file
 *
 * @param data Array of ephemeris data to save
 * @param count Number of objects in the array
 * @param filename Name of the file to save to
 * @return 0 on success, error code on failure
 */
int de430_save_to_binary(const DE430EphemerisData *data, int count, const char *filename) {
    if (!data || count <= 0 || !filename) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Open the file
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return DE430_ERROR_FILE_IO;
    }

    // Write file header
    DE430BinaryHeader header;
    memcpy(header.magic, "DE43", 4);
    header.version = 1;
    header.object_count = count;
    header.reserved = 0;

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return DE430_ERROR_FILE_IO;
    }

    // Write each object
    for (int i = 0; i < count; i++) {
        const DE430EphemerisData *obj = &data[i];

        // Write object header
        DE430BinaryObjectHeader obj_header;
        obj_header.name_length = strlen(obj->object_name) + 1; // Include null terminator
        obj_header.point_count = obj->count;

        if (fwrite(&obj_header, sizeof(obj_header), 1, fp) != 1) {
            fclose(fp);
            return DE430_ERROR_FILE_IO;
        }

        // Write object name
        if (fwrite(obj->object_name, 1, obj_header.name_length, fp) != obj_header.name_length) {
            fclose(fp);
            return DE430_ERROR_FILE_IO;
        }

        // Write each data point
        for (int j = 0; j < obj->count; j++) {
            const DE430EphemerisPoint *point = &obj->points[j];

            // Write fixed part of point
            DE430BinaryPointHeader point_header;
            point_header.jd = point->jd;
            memcpy(point_header.position, point->position, sizeof(point_header.position));
            memcpy(point_header.ra_dec, point->ra_dec, sizeof(point_header.ra_dec));
            point_header.magnitude = point->magnitude;
            point_header.phase = point->phase;
            point_header.angular_size = point->angular_size;
            point_header.physical_size = point->physical_size;
            point_header.albedo = point->albedo;
            point_header.sun_dist = point->sun_dist;
            point_header.earth_dist = point->earth_dist;
            point_header.sun_ang_dist = point->sun_ang_dist;
            point_header.theta_edo = point->theta_edo;
            memcpy(point_header.ecliptic, point->ecliptic, sizeof(point_header.ecliptic));
            point_header.constellation_length = strlen(point->constellation) + 1; // Include null terminator

            if (fwrite(&point_header, sizeof(point_header), 1, fp) != 1) {
                fclose(fp);
                return DE430_ERROR_FILE_IO;
            }

            // Write constellation
            if (fwrite(point->constellation, 1, point_header.constellation_length, fp) !=
                point_header.constellation_length) {
                fclose(fp);
                return DE430_ERROR_FILE_IO;
            }
        }
    }

    fclose(fp);
    return DE430_ERROR_NONE;
}

/**
 * Load ephemeris data from a binary file
 *
 * @param filename Name of the file to load from
 * @param result Pointer to store the resulting data (must be freed with de430_free_data)
 * @param count Number of objects loaded
 * @return 0 on success, error code on failure
 */
int de430_load_from_binary(const char *filename, DE430EphemerisData **result, int *count) {
    if (!filename || !result || !count) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Open the file
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return DE430_ERROR_FILE_IO;
    }

    // Read file header
    DE430BinaryHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return DE430_ERROR_PARSE_FAILED;
    }

    // Verify magic number
    if (memcmp(header.magic, "DE43", 4) != 0) {
        fclose(fp);
        return DE430_ERROR_PARSE_FAILED;
    }

    // Currently, we only support version 1
    if (header.version != 1) {
        fclose(fp);
        return DE430_ERROR_PARSE_FAILED;
    }

    // Allocate result array
    *result = malloc(header.object_count * sizeof(DE430EphemerisData));
    if (!*result) {
        fclose(fp);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Read each object
    for (int i = 0; i < header.object_count; i++) {
        // Initialize object fields
        (*result)[i].points = NULL;

        // Read object header
        DE430BinaryObjectHeader obj_header;
        if (fread(&obj_header, sizeof(obj_header), 1, fp) != 1) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            fclose(fp);
            return DE430_ERROR_PARSE_FAILED;
        }

        // Read object name
        if (obj_header.name_length > sizeof((*result)[i].object_name)) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            fclose(fp);
            return DE430_ERROR_PARSE_FAILED;
        }

        if (fread((*result)[i].object_name, 1, obj_header.name_length, fp) != obj_header.name_length) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            fclose(fp);
            return DE430_ERROR_PARSE_FAILED;
        }

        // Allocate memory for points
        (*result)[i].count = obj_header.point_count;
        (*result)[i].points = malloc(obj_header.point_count * sizeof(DE430EphemerisPoint));

        if (!(*result)[i].points) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            fclose(fp);
            return DE430_ERROR_MEMORY_ALLOCATION;
        }

        // Read each data point
        for (int j = 0; j < obj_header.point_count; j++) {
            DE430EphemerisPoint *point = &((*result)[i].points[j]);

            // Read fixed part of point
            DE430BinaryPointHeader point_header;
            if (fread(&point_header, sizeof(point_header), 1, fp) != 1) {
                // Clean up on failure
                for (int k = 0; k <= i; k++) {
                    free((*result)[k].points);
                }
                free(*result);
                *result = NULL;
                fclose(fp);
                return DE430_ERROR_PARSE_FAILED;
            }

            // Copy fixed data to point
            point->jd = point_header.jd;
            memcpy(point->position, point_header.position, sizeof(point->position));
            memcpy(point->ra_dec, point_header.ra_dec, sizeof(point->ra_dec));
            point->magnitude = point_header.magnitude;
            point->phase = point_header.phase;
            point->angular_size = point_header.angular_size;
            point->physical_size = point_header.physical_size;
            point->albedo = point_header.albedo;
            point->sun_dist = point_header.sun_dist;
            point->earth_dist = point_header.earth_dist;
            point->sun_ang_dist = point_header.sun_ang_dist;
            point->theta_edo = point_header.theta_edo;
            memcpy(point->ecliptic, point_header.ecliptic, sizeof(point->ecliptic));

            // Read constellation
            if (point_header.constellation_length > sizeof(point->constellation)) {
                // Clean up on failure
                for (int k = 0; k <= i; k++) {
                    free((*result)[k].points);
                }
                free(*result);
                *result = NULL;
                fclose(fp);
                return DE430_ERROR_PARSE_FAILED;
            }

            if (fread(point->constellation, 1, point_header.constellation_length, fp) !=
                point_header.constellation_length) {
                // Clean up on failure
                for (int k = 0; k <= i; k++) {
                    free((*result)[k].points);
                }
                free(*result);
                *result = NULL;
                fclose(fp);
                return DE430_ERROR_PARSE_FAILED;
            }
        }
    }

    fclose(fp);

    *count = header.object_count;
    return DE430_ERROR_NONE;
}