#!/bin/bash

BUILD_ENV=$1

# Check if the build environment is provided
if [ -z "$BUILD_ENV" ]; then
    echo "Error: No build environment specified. Use 'prod' or 'dev'."
    exit 1
fi

SRC_FILES=$(find ./src -name "*.c")

if [ "$BUILD_ENV" == "prod" ]; then
    echo "Building for production..."

    BUILD_DIR="./build-$BUILD_ENV"

    rm -rf "$BUILD_DIR"

    # Create the build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
  
    gcc \
        -O3 \
        -std=c89 \
        -o "$BUILD_DIR/app" \
        $SRC_FILES \
        -I./src \
        -I../sqlite-amalgamation-3480000 \
        -L../sqlite-amalgamation-3480000 \
        -lsqlite3 \
        -largon2 \
        -pthread \
        -lcrypto

        set -o allexport && source <(grep '^CMPL__' ".env.$BUILD_ENV") && set +o allexport

        for var in $(env | grep '^CMPL__' | cut -d= -f1); do
            source="${!var}"  # Get the value of the environment variable
            if [ -d "$source" ]; then
                # If the source exists, copy it to $BUILD_DIR
                cp -r "$source" $BUILD_DIR
            elif [ -f "$source" ]; then
                # If the source is a file, copy the individual file
                target_dir="$BUILD_DIR/$(dirname "$source")"
                mkdir -p "$target_dir"
                cp "$source" "$target_dir/"
            else
                echo "Warning: $source does not exist, skipping..."
            fi
        done

        cp ".env.$BUILD_ENV" "$BUILD_DIR/.env.$BUILD_ENV"

elif [ "$BUILD_ENV" == "dev" ]; then
    echo "Building for development..."

    gcc \
        -std=c89 \
        -g \
        -Wall \
        -Wextra \
        -Werror \
        -pedantic \
        -Wno-declaration-after-statement \
        -Wno-unused-variable \
        -Wno-unused-parameter \
        -Wno-long-long \
        -DDEV \
        -o "littlechef-dev" \
        $SRC_FILES \
        -I./src \
        -I../sqlite-amalgamation-3480000 \
        -L../sqlite-amalgamation-3480000 \
        -lsqlite3 \
        -largon2 \
        -pthread \
        -lcrypto

else
    echo "Invalid or no environment specified. Use 'prod' or 'dev'."
    exit 1
fi

echo "Done!"
