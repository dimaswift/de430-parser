# DE430 Ephemeris Parser

This library provides a C interface for https://github.com/dcf21/ephemeris-compute-de430

## Overview

The DE430 Ephemeris Parser offers:

- High-precision solar system object positions using NASA JPL's DE430 data
- Easy configuration of time range, objects, and output formats
- Comprehensive output including positions, RA/Dec, magnitudes, distances, and more
- Multiple output formats (space-separated raw data)
- Docker-based execution that works across platforms

## Requirements

- de430 running Docker image
- C compiler (GCC, Clang, etc.)
- CMake 3.10+
- Docker
- Git

## Installation

### 1. Clone de430 repository and follow instructions to build docker image

```bash
git https://github.com/dcf21/ephemeris-compute-de430
cd ephemeris-compute-de430
```

### 2. Build the parser library

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Basic example

```c
#include "de430_parser.h"
#include <stdio.h>

int main() {
    // Initialize configuration
    DE430Config config;
    de430_init_config(&config);
    
    // Set the date range (J2000.0 to J2000.0 + 10 days)
    config.jd_min = 2451544.5;
    config.jd_max = 2451554.5;
    config.jd_step = 1.0;
    
    // Request Jupiter data in extended format
    strcpy(config.objects, "jupiter");
    config.output_format = 3;  // Extended format
    config.output_constellations = 1;  // Include constellation info
    
    // Get ephemeris data
    DE430EphemerisData *data = NULL;
    int object_count = 0;
    
    printf("Requesting ephemeris data...\n");
    int status = de430_get_ephemeris(&config, &data, &object_count);
    
    if (status != 0) {
        printf("Error: %s\n", de430_get_error(status));
        return 1;
    }
    
    printf("Received %d objects\n", object_count);
    
    // Print data for each object
    for (int i = 0; i < object_count; i++) {
        printf("Object: %s (%d points)\n", data[i].object_name, data[i].count);
        
        // Print first data point
        if (data[i].count > 0) {
            printf("  JD: %.1f, Position: [%.6f, %.6f, %.6f]\n",
                data[i].points[0].jd,
                data[i].points[0].position[0],
                data[i].points[0].position[1],
                data[i].points[0].position[2]);
            
            if (strlen(data[i].points[0].constellation) > 0) {
                printf("  Constellation: %s\n", data[i].points[0].constellation);
            }
        }
    }
    
    // Free the data
    de430_free_data(data, object_count);
    
    return 0;
}
```

### Multiple objects

```c
// Request multiple planets
strcpy(config.objects, "mercury,venus,earth,mars,jupiter,saturn,uranus,neptune");
```

### Custom time range with specific dates

```c
// Set specific Julian dates
double jd_list[] = {2451545.0, 2451575.0, 2451605.0}; // J2000.0, +30 days, +60 days
config.jd_list = jd_list;
config.jd_list_count = 3;
```

### Topocentric correction (observer location)

```c
// Set observer's location (e.g., New York City)
config.latitude = 40.7128;
config.longitude = -74.0060;
config.enable_topocentric = 1;
```

## Docker Commands

If you need to run the ephemeris calculator directly:

```bash
# Interactive mode
docker run -it ephemeris-compute-de430:v6 /bin/bash

# Direct execution
docker run --rm ephemeris-compute-de430:v6 ./bin/ephem.bin --objects "jupiter" --jd_min 2451544.5 --jd_max 2451545.5
```

## API Reference

### Structures

#### DE430Config

Configuration for ephemeris requests:

```c
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
    int output_format;          // Output format (0-3)
    int use_orbital_elements;   // Whether to use orbital elements
    int output_constellations;  // Whether to include constellations
} DE430Config;
```

#### DE430EphemerisPoint

Individual data point for a celestial object:

```c
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
```

#### DE430EphemerisData

Collection of data points for an object:

```c
typedef struct {
    DE430EphemerisPoint *points;  // Array of data points
    int count;                    // Number of data points in the array
    char object_name[64];         // Name of the astronomical object
} DE430EphemerisData;
```

### Functions

#### de430_init_config

```c
void de430_init_config(DE430Config *config);
```

Initialize a DE430Config structure with default values.

#### de430_get_ephemeris

```c
int de430_get_ephemeris(const DE430Config *config, DE430EphemerisData **result, int *count);
```

Calculate ephemeris data according to the specified configuration.

#### de430_free_data

```c
void de430_free_data(DE430EphemerisData *data, int count);
```

Free memory allocated for ephemeris data.

#### de430_get_error

```c
const char* de430_get_error(int error_code);
```

Get an error message for a given error code.

## Error Codes

- `DE430_ERROR_NONE` (0): Success
- `DE430_ERROR_COMMAND_FAILED` (-1): Docker command execution failed
- `DE430_ERROR_MEMORY_ALLOCATION` (-2): Memory allocation failed
- `DE430_ERROR_PARSE_FAILED` (-3): Failed to parse output data
- `DE430_ERROR_INVALID_CONFIG` (-4): Invalid configuration

## Output Formats

The DE430 library supports multiple output formats specified by `config.output_format`:

- `0`: Position only (XYZ coordinates)
- `1`: Position + RA/Dec
- `2`: Position + RA/Dec + basic parameters
- `3`: Extended format with all fields

## Troubleshooting

### Docker Issues

If you encounter problems with Docker:

1. Verify Docker is installed and running:
   ```bash
   docker --version
   docker ps
   ```

2. Check Docker image:
   ```bash
   docker images | grep ephemeris-compute-de430
   ```

3. Test the Docker container directly:
   ```bash
   docker run --rm ephemeris-compute-de430:v6 ./bin/ephem.bin --help
   ```

### Parser Issues

1. Set up debug output:
   ```c
   printf("Executing command: %s\n", ephemeris_command);
   ```

2. Check Docker command output:
   ```bash
   docker run --rm ephemeris-compute-de430:v6 ./bin/ephem.bin --objects "jupiter" --jd_min 2451544.5 --jd_max 2451545.5
   ```

3. Verify file permissions for temporary files

## Acknowledgments

- NASA JPL for the DE430 ephemeris data
- Contributors to the ephemeris-compute project (Dominic Ford)