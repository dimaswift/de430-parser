//
// Created by Dmitry Popov on 18.05.2025.
//
#include "de430_parser.h"
#include "cJSON.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

static cJSON* ephemeris_point_to_json(const DE430EphemerisPoint *point);
static cJSON* ephemeris_data_to_json(const DE430EphemerisData *data);
static int json_to_ephemeris_point(cJSON *json, DE430EphemerisPoint *point);
static int json_to_ephemeris_data(cJSON *json, DE430EphemerisData *data);

int de430_save_to_json(const DE430EphemerisData *data, int count, const char *filename) {
    if (!data || count <= 0 || !filename) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Create a root JSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Add count
    cJSON_AddNumberToObject(root, "object_count", count);

    // Create objects array
    cJSON *objects = cJSON_CreateArray();
    if (!objects) {
        cJSON_Delete(root);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Add each object
    for (int i = 0; i < count; i++) {
        cJSON *object = ephemeris_data_to_json(&data[i]);
        if (!object) {
            cJSON_Delete(root);
            return DE430_ERROR_MEMORY_ALLOCATION;
        }

        cJSON_AddItemToArray(objects, object);
    }

    cJSON_AddItemToObject(root, "objects", objects);

    // Convert to string
    char *json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Write to file
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        free(json_str);
        cJSON_Delete(root);
        return DE430_ERROR_FILE_IO;
    }

    fprintf(fp, "%s", json_str);

    fclose(fp);
    free(json_str);
    cJSON_Delete(root);

    return DE430_ERROR_NONE;
}

int de430_load_from_json(const char *filename, DE430EphemerisData **result, int *count) {
    if (!filename || !result || !count) {
        return DE430_ERROR_INVALID_CONFIG;
    }

    // Open the file
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return DE430_ERROR_FILE_IO;
    }

    // Get the file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Allocate memory for the file contents
    char *json_str = (char*)malloc(file_size + 1);
    if (!json_str) {
        fclose(fp);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Read the file
    size_t read_size = fread(json_str, 1, file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size) {
        free(json_str);
        return DE430_ERROR_FILE_IO;
    }

    // Null-terminate the string
    json_str[file_size] = '\0';

    // Parse the JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        return DE430_ERROR_JSON_PARSE;
    }

    // Get object count
    cJSON *object_count = cJSON_GetObjectItem(root, "object_count");
    if (!cJSON_IsNumber(object_count)) {
        cJSON_Delete(root);
        return DE430_ERROR_JSON_PARSE;
    }

    *count = (int)object_count->valuedouble;

    // Get objects array
    cJSON *objects = cJSON_GetObjectItem(root, "objects");
    if (!cJSON_IsArray(objects) || cJSON_GetArraySize(objects) != *count) {
        cJSON_Delete(root);
        return DE430_ERROR_JSON_PARSE;
    }

    // Allocate memory for the result
    *result = (DE430EphemerisData*)malloc(*count * sizeof(DE430EphemerisData));
    if (!*result) {
        cJSON_Delete(root);
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Parse each object
    for (int i = 0; i < *count; i++) {
        cJSON *object = cJSON_GetArrayItem(objects, i);
        if (!object) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            cJSON_Delete(root);
            return DE430_ERROR_JSON_PARSE;
        }

        int status = json_to_ephemeris_data(object, &(*result)[i]);
        if (status != DE430_ERROR_NONE) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free((*result)[j].points);
            }
            free(*result);
            *result = NULL;
            cJSON_Delete(root);
            return status;
        }
    }

    cJSON_Delete(root);

    return DE430_ERROR_NONE;
}

// JSON conversion functions

static cJSON* ephemeris_point_to_json(const DE430EphemerisPoint *point) {
    if (!point) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    // Add basic fields
    cJSON_AddNumberToObject(json, "jd", point->jd);
    cJSON_AddNumberToObject(json, "magnitude", point->magnitude);
    cJSON_AddNumberToObject(json, "phase", point->phase);
    cJSON_AddNumberToObject(json, "angular_size", point->angular_size);
    cJSON_AddNumberToObject(json, "physical_size", point->physical_size);
    cJSON_AddNumberToObject(json, "albedo", point->albedo);
    cJSON_AddNumberToObject(json, "sun_dist", point->sun_dist);
    cJSON_AddNumberToObject(json, "earth_dist", point->earth_dist);
    cJSON_AddNumberToObject(json, "sun_ang_dist", point->sun_ang_dist);
    cJSON_AddNumberToObject(json, "theta_edo", point->theta_edo);
    cJSON_AddStringToObject(json, "constellation", point->constellation);

    // Add position as array
    cJSON *position = cJSON_CreateArray();
    if (!position) {
        cJSON_Delete(json);
        return NULL;
    }

    for (int i = 0; i < 3; i++) {
        cJSON_AddItemToArray(position, cJSON_CreateNumber(point->position[i]));
    }
    cJSON_AddItemToObject(json, "position", position);

    // Add RA/Dec as array
    cJSON *ra_dec = cJSON_CreateArray();
    if (!ra_dec) {
        cJSON_Delete(json);
        return NULL;
    }

    for (int i = 0; i < 2; i++) {
        cJSON_AddItemToArray(ra_dec, cJSON_CreateNumber(point->ra_dec[i]));
    }
    cJSON_AddItemToObject(json, "ra_dec", ra_dec);

    // Add ecliptic as array
    cJSON *ecliptic = cJSON_CreateArray();
    if (!ecliptic) {
        cJSON_Delete(json);
        return NULL;
    }

    for (int i = 0; i < 3; i++) {
        cJSON_AddItemToArray(ecliptic, cJSON_CreateNumber(point->ecliptic[i]));
    }
    cJSON_AddItemToObject(json, "ecliptic", ecliptic);

    return json;
}

static cJSON* ephemeris_data_to_json(const DE430EphemerisData *data) {
    if (!data) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    // Add object name and count
    cJSON_AddStringToObject(json, "object_name", data->object_name);
    cJSON_AddNumberToObject(json, "count", data->count);

    // Create points array
    cJSON *points = cJSON_CreateArray();
    if (!points) {
        cJSON_Delete(json);
        return NULL;
    }

    // Add each point
    for (int i = 0; i < data->count; i++) {
        cJSON *point = ephemeris_point_to_json(&data->points[i]);
        if (!point) {
            cJSON_Delete(json);
            return NULL;
        }
        cJSON_AddItemToArray(points, point);
    }

    cJSON_AddItemToObject(json, "points", points);

    return json;
}

static int json_to_ephemeris_point(cJSON *json, DE430EphemerisPoint *point) {
    if (!json || !point) return DE430_ERROR_INVALID_CONFIG;

    // Clear the point
    memset(point, 0, sizeof(DE430EphemerisPoint));

    // Get basic fields
    cJSON *jd = cJSON_GetObjectItem(json, "jd");
    if (cJSON_IsNumber(jd)) {
        point->jd = jd->valuedouble;
    }

    cJSON *magnitude = cJSON_GetObjectItem(json, "magnitude");
    if (cJSON_IsNumber(magnitude)) {
        point->magnitude = magnitude->valuedouble;
    }

    cJSON *phase = cJSON_GetObjectItem(json, "phase");
    if (cJSON_IsNumber(phase)) {
        point->phase = phase->valuedouble;
    }

    cJSON *angular_size = cJSON_GetObjectItem(json, "angular_size");
    if (cJSON_IsNumber(angular_size)) {
        point->angular_size = angular_size->valuedouble;
    }

    cJSON *physical_size = cJSON_GetObjectItem(json, "physical_size");
    if (cJSON_IsNumber(physical_size)) {
        point->physical_size = physical_size->valuedouble;
    }

    cJSON *albedo = cJSON_GetObjectItem(json, "albedo");
    if (cJSON_IsNumber(albedo)) {
        point->albedo = albedo->valuedouble;
    }

    cJSON *sun_dist = cJSON_GetObjectItem(json, "sun_dist");
    if (cJSON_IsNumber(sun_dist)) {
        point->sun_dist = sun_dist->valuedouble;
    }

    cJSON *earth_dist = cJSON_GetObjectItem(json, "earth_dist");
    if (cJSON_IsNumber(earth_dist)) {
        point->earth_dist = earth_dist->valuedouble;
    }

    cJSON *sun_ang_dist = cJSON_GetObjectItem(json, "sun_ang_dist");
    if (cJSON_IsNumber(sun_ang_dist)) {
        point->sun_ang_dist = sun_ang_dist->valuedouble;
    }

    cJSON *theta_edo = cJSON_GetObjectItem(json, "theta_edo");
    if (cJSON_IsNumber(theta_edo)) {
        point->theta_edo = theta_edo->valuedouble;
    }

    cJSON *constellation = cJSON_GetObjectItem(json, "constellation");
    if (cJSON_IsString(constellation)) {
        strncpy(point->constellation, constellation->valuestring, sizeof(point->constellation) - 1);
        point->constellation[sizeof(point->constellation) - 1] = '\0';
    }

    // Get position array
    cJSON *position = cJSON_GetObjectItem(json, "position");
    if (cJSON_IsArray(position)) {
        for (int i = 0; i < 3 && i < cJSON_GetArraySize(position); i++) {
            cJSON *item = cJSON_GetArrayItem(position, i);
            if (cJSON_IsNumber(item)) {
                point->position[i] = item->valuedouble;
            }
        }
    }

    // Get RA/Dec array
    cJSON *ra_dec = cJSON_GetObjectItem(json, "ra_dec");
    if (cJSON_IsArray(ra_dec)) {
        for (int i = 0; i < 2 && i < cJSON_GetArraySize(ra_dec); i++) {
            cJSON *item = cJSON_GetArrayItem(ra_dec, i);
            if (cJSON_IsNumber(item)) {
                point->ra_dec[i] = item->valuedouble;
            }
        }
    }

    // Get ecliptic array
    cJSON *ecliptic = cJSON_GetObjectItem(json, "ecliptic");
    if (cJSON_IsArray(ecliptic)) {
        for (int i = 0; i < 3 && i < cJSON_GetArraySize(ecliptic); i++) {
            cJSON *item = cJSON_GetArrayItem(ecliptic, i);
            if (cJSON_IsNumber(item)) {
                point->ecliptic[i] = item->valuedouble;
            }
        }
    }

    return DE430_ERROR_NONE;
}

static int json_to_ephemeris_data(cJSON *json, DE430EphemerisData *data) {
    if (!json || !data) return DE430_ERROR_INVALID_CONFIG;

    // Clear the data
    memset(data, 0, sizeof(DE430EphemerisData));

    // Get object name
    cJSON *object_name = cJSON_GetObjectItem(json, "object_name");
    if (cJSON_IsString(object_name)) {
        strncpy(data->object_name, object_name->valuestring, sizeof(data->object_name) - 1);
        data->object_name[sizeof(data->object_name) - 1] = '\0';
    }

    // Get points array
    cJSON *points = cJSON_GetObjectItem(json, "points");
    if (!cJSON_IsArray(points)) {
        return DE430_ERROR_JSON_PARSE;
    }

    // Allocate memory for points
    int point_count = cJSON_GetArraySize(points);
    data->points = (DE430EphemerisPoint*)malloc(point_count * sizeof(DE430EphemerisPoint));
    if (!data->points) {
        return DE430_ERROR_MEMORY_ALLOCATION;
    }

    // Parse each point
    for (int i = 0; i < point_count; i++) {
        cJSON *point = cJSON_GetArrayItem(points, i);
        if (!point) {
            free(data->points);
            data->points = NULL;
            return DE430_ERROR_JSON_PARSE;
        }

        int status = json_to_ephemeris_point(point, &data->points[i]);
        if (status != DE430_ERROR_NONE) {
            free(data->points);
            data->points = NULL;
            return status;
        }
    }

    data->count = point_count;

    return DE430_ERROR_NONE;
}

