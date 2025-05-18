//
// Created by Dmitry Popov on 18.05.2025.
//

#include "de430_parser.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Static error messages
static const char *error_messages[] = {
    "Success",
    "Docker command execution failed",
    "Memory allocation failed",
    "Failed to parse output data",
    "Invalid configuration"
};

// Internal functions

static FILE* execute_docker_command(const char *command);
static int parse_ephemeris_output(FILE *fp, const DE430Config *config,
                                 DE430EphemerisData **result, int *count);
static void split_objects_string(const char *objects, char ***object_names, int *object_count);

void de430_init_config(DE430Config *config) {
    if (!config) return;

    // Initialize with default values
    memset(config, 0, sizeof(DE430Config));
    config->jd_min = 2451544.5;       // J2000.0
    config->jd_max = 2451574.5;       // J2000.0 + 30 days
    config->jd_step = 1.0;            // 1 day step
    config->jd_list = NULL;
    config->jd_list_count = 0;
    config->latitude = 0.0;
    config->longitude = 0.0;
    config->enable_topocentric = 0;
    config->epoch = 2451545.0;        // J2000.0
    strcpy(config->objects, "jupiter");
    config->output_format = 0;        // XYZ ICRS coordinates
    config->use_orbital_elements = 0;
    config->output_constellations = 0;
}

// Error codes and buffer sizes remain the same

// Modified to build a command to be executed inside the container
static char* build_ephemeris_command(const DE430Config *config) {
    // Allocate a buffer for the command
    char *command = (char*)malloc(COMMAND_BUFFER_SIZE);
    if (!command) return NULL;

    // Start with the base ephemeris command (no docker part)
  //  strcpy(command, "/bin/ephem.bin ");

    // Add arguments based on configuration
    char buffer[256];

    // JD min/max/step
    if (config->jd_list == NULL || config->jd_list_count == 0) {
        sprintf(buffer, "--jd_min %.15f --jd_max %.15f --jd_step %.15f ",
                config->jd_min, config->jd_max, config->jd_step);
        strcat(command, buffer);
    } else {
        // Format the jd_list
        strcat(command, "--jd_list \"");
        for (int i = 0; i < config->jd_list_count; i++) {
            sprintf(buffer, "%.15f", config->jd_list[i]);
            strcat(command, buffer);
            if (i < config->jd_list_count - 1) {
                strcat(command, ",");
            }
        }
        strcat(command, "\" ");
    }

    // Topocentric correction
    if (config->enable_topocentric) {
        sprintf(buffer, "--latitude %.6f --longitude %.6f --enable_topocentric_correction 1 ",
                config->latitude, config->longitude);
        strcat(command, buffer);
    }

    // Epoch
    sprintf(buffer, "--epoch %.15f ", config->epoch);
    strcat(command, buffer);

    // Objects
    sprintf(buffer, "--objects \"%s\" ", config->objects);
    strcat(command, buffer);

    // Output format
    sprintf(buffer, "--output_format %d ", config->output_format);
    strcat(command, buffer);

    // Use orbital elements
    sprintf(buffer, "--use_orbital_elements %d ", config->use_orbital_elements);
    strcat(command, buffer);

    // Output constellations
    sprintf(buffer, "--output_constellations %d", config->output_constellations);
    strcat(command, buffer);

    return command;
}

static FILE* execute_docker_command(const char *ephemeris_command) {
    // Build the full command string
    char full_command[COMMAND_BUFFER_SIZE + 100];
    sprintf(full_command, "docker run --rm ephemeris-compute-de430:v6 ./bin/ephem.bin %s", ephemeris_command);

    // Print the command for debugging
    printf("Executing command: %s\n", full_command);

    // Execute the command directly
    FILE *pipe = popen(full_command, "r");

    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute Docker command\n");
    }

    return pipe;
}

static char* build_docker_command(const DE430Config *config) {
    // Allocate a buffer for the command
    char *command = (char*)malloc(COMMAND_BUFFER_SIZE);
    if (!command) return NULL;

    // Start with the base docker command
    strcpy(command, "docker compose run ephemeris-compute-de430 ");

    // Add arguments based on configuration
    char buffer[256];

    // JD min/max/step
    if (config->jd_list == NULL || config->jd_list_count == 0) {
        sprintf(buffer, "--jd_min %.15f --jd_max %.15f --jd_step %.15f ",
                config->jd_min, config->jd_max, config->jd_step);
        strcat(command, buffer);
    } else {
        // Format the jd_list
        strcat(command, "--jd_list \"");
        for (int i = 0; i < config->jd_list_count; i++) {
            sprintf(buffer, "%.15f", config->jd_list[i]);
            strcat(command, buffer);
            if (i < config->jd_list_count - 1) {
                strcat(command, ",");
            }
        }
        strcat(command, "\" ");
    }

    // Topocentric correction
    if (config->enable_topocentric) {
        sprintf(buffer, "--latitude %.6f --longitude %.6f --enable_topocentric_correction 1 ",
                config->latitude, config->longitude);
        strcat(command, buffer);
    }

    // Epoch
    sprintf(buffer, "--epoch %.15f ", config->epoch);
    strcat(command, buffer);

    // Objects
    sprintf(buffer, "--objects \"%s\" ", config->objects);
    strcat(command, buffer);

    // Output format
    sprintf(buffer, "--output_format %d ", config->output_format);
    strcat(command, buffer);

    // Use orbital elements
    sprintf(buffer, "--use_orbital_elements %d ", config->use_orbital_elements);
    strcat(command, buffer);

    // Output constellations
    sprintf(buffer, "--output_constellations %d", config->output_constellations);
    strcat(command, buffer);

    return command;
}


static void split_objects_string(const char *objects, char ***object_names, int *object_count) {
    // Count the number of objects (comma-separated)
    int count = 1;
    for (const char *p = objects; *p; p++) {
        if (*p == ',') count++;
    }

    // Allocate memory for the array of object names
    *object_names = (char**)malloc(count * sizeof(char*));
    if (!*object_names) {
        *object_count = 0;
        return;
    }

    // Make a copy of the objects string so we can modify it
    char *objects_copy = strdup(objects);
    if (!objects_copy) {
        free(*object_names);
        *object_names = NULL;
        *object_count = 0;
        return;
    }

    // Split the string by commas
    int i = 0;
    char *token = strtok(objects_copy, ",");
    while (token != NULL && i < count) {
        // Skip leading whitespace
        while (*token == ' ') token++;

        // Allocate memory for the object name
        (*object_names)[i] = strdup(token);
        i++;

        token = strtok(NULL, ",");
    }

    *object_count = i;
    free(objects_copy);
}

// Parse data using JSON intermediary
static int parse_ephemeris_output(FILE *fp, const DE430Config *config,
                                 DE430EphemerisData **result, int *count) {
    if (!fp || !config || !result || !count) {
        return DE430_ERROR_INVALID_CONFIG;
    }


    // Reset file pointer
    rewind(fp);

    // Split the objects string to get individual object names
    char **object_names = NULL;
    int object_count = 0;
    split_objects_string(config->objects, &object_names, &object_count);

    if (object_count == 0) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Allocate memory for the result
    *result = (DE430EphemerisData*)malloc(object_count * sizeof(DE430EphemerisData));
    if (!*result) {
        for (int i = 0; i < object_count; i++) {
            free(object_names[i]);
        }
        free(object_names);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Initialize result structures
    for (int i = 0; i < object_count; i++) {
        (*result)[i].points = NULL;
        (*result)[i].count = 0;
        strncpy((*result)[i].object_name, object_names[i], sizeof((*result)[i].object_name) - 1);
        (*result)[i].object_name[sizeof((*result)[i].object_name) - 1] = '\0';
    }

    // Create arrays to store point data for each object
    int capacity = INITIAL_RESULTS_SIZE;
    for (int i = 0; i < object_count; i++) {
        (*result)[i].points = (DE430EphemerisPoint*)malloc(
            capacity * sizeof(DE430EphemerisPoint));

        if (!(*result)[i].points) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;

            for (int j = 0; j < object_count; j++) {
                free(object_names[j]);
            }
            free(object_names);

            return DE430_ERROR_MEMORY_ALLOCATION;
        }
    }

    // Read the file line by line
    char line[LINE_BUFFER_SIZE];
    int line_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines
        if (line[0] == '\n' || line[0] == '\0') continue;

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Check if we need to resize the arrays
        if (line_count >= capacity) {
            capacity *= 2;
            for (int i = 0; i < object_count; i++) {
                DE430EphemerisPoint *new_points = (DE430EphemerisPoint*)realloc(
                    (*result)[i].points, capacity * sizeof(DE430EphemerisPoint));

                if (!new_points) {
                    // Clean up on failure
                    for (int j = 0; j < object_count; j++) {
                        free((*result)[j].points);
                    }
                    free(*result);
                    *result = NULL;

                    for (int j = 0; j < object_count; j++) {
                        free(object_names[j]);
                    }
                    free(object_names);

                    return DE430_ERROR_MEMORY_ALLOCATION;
                }

                (*result)[i].points = new_points;
            }
        }

        // Use JSON to parse the line
        cJSON *line_json = cJSON_CreateObject();
        if (!line_json) {
            // Clean up on failure
            for (int j = 0; j < object_count; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;

            for (int j = 0; j < object_count; j++) {
                free(object_names[j]);
            }
            free(object_names);

            return DE430_ERROR_MEMORY_ALLOCATION;
        }

        // Split the line into tokens
        char *token;
        char *line_copy = strdup(line);
        char *saveptr = NULL;

        // Get Julian date (first token)
        token = strtok_r(line_copy, " \t", &saveptr);
        if (!token) {
            free(line_copy);
            cJSON_Delete(line_json);
            continue; // Skip malformed lines
        }

        double julian_date = atof(token);

        // Parse the remaining tokens for each object
        for (int i = 0; i < object_count; i++) {
            DE430EphemerisPoint *point = &((*result)[i].points[line_count]);
            memset(point, 0, sizeof(DE430EphemerisPoint));

            // Set the Julian date
            point->jd = julian_date;

            // Position (XYZ)
            for (int j = 0; j < 3; j++) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (!token) {
                    fprintf(stderr, "Warning: Not enough tokens for position[%d] in object %d\n", j, i);
                    break;
                }
                point->position[j] = atof(token);
            }

            // RA/Dec
            for (int j = 0; j < 2; j++) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (!token) {
                    fprintf(stderr, "Warning: Not enough tokens for ra_dec[%d] in object %d\n", j, i);
                    break;
                }
                point->ra_dec[j] = atof(token);
            }

            // Magnitude
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->magnitude = atof(token);

            // Phase
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->phase = atof(token);

            // Angular size
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->angular_size = atof(token);

            // Physical size
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->physical_size = atof(token);

            // Albedo
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->albedo = atof(token);

            // Sun distance
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->sun_dist = atof(token);

            // Earth distance
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->earth_dist = atof(token);

            // Sun angular distance
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->sun_ang_dist = atof(token);

            // Theta EDO
            token = strtok_r(NULL, " \t", &saveptr);
            if (!token) break;
            point->theta_edo = atof(token);

            // Ecliptic coordinates
            for (int j = 0; j < 3; j++) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (!token) break;
                point->ecliptic[j] = atof(token);
            }

            // Constellation (if enabled)
            if (config->output_constellations) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (token) {
                    strncpy(point->constellation, token, sizeof(point->constellation) - 1);
                    point->constellation[sizeof(point->constellation) - 1] = '\0';
                }
            }
        }

        free(line_copy);
        cJSON_Delete(line_json);
        line_count++;
    }

    // Update the count for each object
    for (int i = 0; i < object_count; i++) {
        (*result)[i].count = line_count;
    }

    // Clean up object names
    for (int i = 0; i < object_count; i++) {
        free(object_names[i]);
    }
    free(object_names);

    *count = object_count;
    return DE430_ERROR_NONE;
}


int de430_get_ephemeris(const DE430Config *config, DE430EphemerisData **result, int *count) {
    if (!config || !result || !count) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Build the ephemeris command
    char *ephemeris_command = build_ephemeris_command(config);
    if (!ephemeris_command) {
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Execute the Docker command
    FILE *fp = execute_docker_command(ephemeris_command);
    free(ephemeris_command);

    if (!fp) {
        return DE430_ERROR_COMMAND_FAILED;
    }

    // Parse the output and fill the result structure
    int status = parse_ephemeris_output(fp, config, result, count);

    // Close the pipe
    pclose(fp);

    return status;
}

void de430_free_data(DE430EphemerisData *data, int count) {
    if (!data) return;

    for (int i = 0; i < count; i++) {
        free(data[i].points);
    }

    free(data);
}

const char* de430_get_error(int error_code) {
    // Convert negative error code to index
    int index = -error_code;

    if (index < 0 || index >= (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return "Unknown error";
    }

    return error_messages[index];
}