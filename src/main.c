#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

const char* kSupportedClilocs[] = {
    "CHS",
    "CHT",
    "DEU",
    "ENU",
    "ESP",
    "FRA",
    "ITA",
    "JPN",
    "KOR",
    "PTB",
    "RUS",
};

enum FILETYPES {
    FILE_UNKNOWN = 0,
    FILE_CSV     = 1,
    FILE_CLILOC  = 2,
};

int copy_content(FILE* a_from, FILE* a_to) {
    if (fseek(a_from, 0, SEEK_END) != 0) {
        return -1;
    }
    size_t n = ftell(a_from);
    if (n == 0) {
        return -1;
    }
    if (fseek(a_from, 0, SEEK_SET) != 0
        || fseek(a_to, 0, SEEK_SET) != 0) {
        return -1;
    }

    const size_t kWriteBufferSize = 1024 * 1024;
    char writeBuffer[kWriteBufferSize];
    size_t written = 0;
    while (written < n) {
        size_t chunkSize = kWriteBufferSize;
        if (chunkSize > (n - written)) {
            chunkSize = (n - written);
        }
        if (fread(writeBuffer, chunkSize, 1, a_from) != 1) {
            return -1;
        }
        if (fwrite(writeBuffer, chunkSize, 1, a_to) != 1) {
            return -1;
        }
        written += chunkSize;
    }
    return 0;
}

void print_usage() {
    fprintf(stdout, "\nUsage:\n");
    fprintf(stdout, "  cliloc <input> <output>\n");
    fprintf(stdout, "    input and output must be valid file paths.\n");
    fprintf(stdout, "    the input file must exist.\n");
    fprintf(stdout, "    to convert cliloc to csv run:\n");
    fprintf(stdout, "      cliloc cliloc.deu german.csv:.\n");
    fprintf(stdout, "    to convert csv to cliloc:\n");
    fprintf(stdout, "      cliloc german.csv cliloc.deu:.\n");
}

char to_lower(char a_ch) {
    if (a_ch >= 'A' && a_ch <= 'Z') {
        a_ch = 'a' + (a_ch - 'A');
    }
    return a_ch;
 }

int compare_extension(const char* a_fileExt, const char* a_ext) {
    while(*a_fileExt == '.') {
        ++a_fileExt;
    }
    int nFileExt = strlen(a_fileExt);
    int nExt = strlen(a_ext);
    if (nFileExt != nExt) {
        return -1;
    }
    for (int i = 0; i < nExt; ++i) {
        if (to_lower(a_fileExt[i]) != to_lower(a_ext[i])) {
            return -1;
        }
    }
    return 0;
}

int get_file_type(const char* a_path) {
    const char* ext = strrchr(a_path, '.');
    if (!ext) {
        return FILE_UNKNOWN;
    }
    const char* sepWin = strrchr(a_path, '\\');
    const char* sepPosix = strrchr(a_path, '/');
    if ((sepPosix && ext < sepPosix)
        || (sepWin && ext < sepWin)) {
        return FILE_UNKNOWN;
    }

    if (compare_extension(ext, "csv") == 0) {
        return FILE_CSV;
    }

    for (int i = 0; i < sizeof(kSupportedClilocs) / sizeof(kSupportedClilocs[0]); ++i) {
        if (compare_extension(ext, kSupportedClilocs[i]) == 0) {
            return FILE_CLILOC;
        }
    }

    return FILE_UNKNOWN;
}

int cliloc_to_csv(const char* a_input, const char* a_output) {
    fprintf(stdout, "%s (cliloc) => %s (csv):\n", a_input, a_output);
    int result = 0;
    size_t n = 0;
    FILE* fCliloc = NULL;
    FILE* fTmp = NULL;
    FILE* fCsv = NULL;

    // open input stream
    if (result == 0) {
        fCliloc = fopen(a_input, "rb");
        if (!fCliloc) {
            fprintf(stderr, "Could not open input file!\n");
            result = -1;
        }
    }
    // basic input stream validation
    if (result == 0) {
        if (fseek(fCliloc, 0, SEEK_END) != 0) {
            fprintf(stderr, "Could not determine input file size!\n");
            result = -1;
        } else {
            n = ftell(fCliloc);
            if (n < 6) {
                fprintf(stderr, "Input file is too small!\n");
                result = -1;
            }
        }
        if (fseek(fCliloc, 6, SEEK_SET) != 0) {
            fprintf(stderr, "Could not rewind input file!\n");
            result = -1;
        }
    }
    // open temporary scratch stream
    if (result == 0) {
        fTmp = tmpfile();
        if (!fTmp) {
            fprintf(stderr, "Could not open temporary file stream!\n");
            result = -1;
        }
    }
    // parse cliloc entries and write to scratch stream
    while (ftell(fCliloc) < n) {
        uint32_t id = 0;
        uint8_t flag = 0;
        uint16_t len = 0;
        // some text strings in non-latin script can be quite long, so let's use a full MB for the string buffer
        const size_t kMaxStringBuffer = 1024 * 1024;
        char buffer[kMaxStringBuffer];
        if (fread(&id, sizeof(uint32_t), 1, fCliloc) != 1) {
            fprintf(stderr, "Error reading ID of cliloc entry! (@%li)\n", ftell(fCliloc));
            result = -1;
            break;
        }
        if (fread(&flag, sizeof(uint8_t), 1, fCliloc) != 1) {
            fprintf(stderr, "Error reading flags of cliloc entry (%u @%li)!\n", id, ftell(fCliloc));
            result = -1;
            break;
        }
        if (fread(&len, sizeof(uint16_t), 1, fCliloc) != 1) {
            fprintf(stderr, "Error reading length of cliloc entry (%u @%li)!\n", id, ftell(fCliloc));
            result = -1;
            break;
        }
        if (len >= kMaxStringBuffer) {
            fprintf(stderr, "Cliloc entry to too large to read (%u @%li)!\n", id, ftell(fCliloc));
            result = -1;
            break;
        } else if (len == 0) {
            buffer[0] = '\0';
            fprintf(stderr, "Zero length string detected: %u. Still writing to csv.\n", id);
        }
        if (len > 0
            && fread(buffer, len, 1, fCliloc) != 1) {
            fprintf(stderr, "Error reading cliloc string (%u @%li)!\n", id, ftell(fCliloc));
            result = -1;
            break;
        }
        buffer[len] = '\0';
        char* lb = strchr(buffer, '\n');
        if (lb && lb < (buffer + len)) {
            fprintf(stderr, "Cliloc string contains line break (%u:%s)!\n", id, buffer);
            result = -1;
            break;
        }
        char* cr = strchr(buffer, '\n');
        if (cr && cr < (buffer + len)) {
            fprintf(stderr, "Cliloc string contains carriage return (%u:%s)!\n", id, buffer);
            result = -1;
            break;
        }
        fprintf(fTmp, "%u;%s;%d\n", id, buffer, flag);
    }
    // open output stream
    if (result == 0) {
        fCsv = fopen(a_output, "w");
        if (!fCsv) {
            fprintf(stderr, "Could not open output file!\n");
            result = -1;
        }
    }
    // copy scratch stream to output stream
    if (result == 0) {
        if (copy_content(fTmp, fCsv) != 0) {
            fprintf(stderr, "Could not write output file!\n");
            result = -1;
        }
    }

    if (result == 0) {
        fprintf(stdout, "  success.\n");
    }

    // cleanup
    if (fCsv) {
        fclose(fCsv);
    }
    if (fTmp) {
        fclose(fTmp);
    }
    if (fCliloc) {
        fclose(fCliloc);
    }

    return result;
}

int csv_to_cliloc(const char* a_input, const char* a_output) {
    fprintf(stdout, "%s (csv) => %s (cliloc):\n", a_input, a_output);
    int result = 0;
    FILE* fCsv = NULL;
    FILE* fTmp = NULL;
    FILE* fCliloc = NULL;

    // open input stream
    if (result == 0) {
        fCsv = fopen(a_input, "rb");
        if (!fCsv) {
            fprintf(stderr, "Could not open input file!\n");
            result = -1;
        }
    }
    // open temporary scratch stream
    if (result == 0) {
        fTmp = tmpfile();
        if (!fTmp) {
            fprintf(stderr, "Could not open temporary file stream!\n");
            result = -1;
        }
    }
    // write header
    if (result == 0) {
        const char header[] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };
        if (fwrite(header, 6, 1, fTmp) != 1) {
            fprintf(stderr, "Could not write header to temporary file stream!\n");
            result = -1;
        }
    }
    int iLine = 0;
    // read input file line by line and write
    if (result == 0) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, fCsv)) >= 0) {
            char* sepId = strchr(line, ';');
            char* sepFlag = strrchr(line, ';');
            if (!sepId || !sepFlag || sepId == sepFlag || sepId == line) {
                fprintf(stderr, "Invalid input line: %s!\n", line);
                result = -1;
            }

            char idBuffer[16] = {};
            size_t idBufferN = (sepId - line - 1) < 16 ? (sepId - line - 1) : 16;
            strncpy(idBuffer, line, idBufferN);
            char* idBufferEnd;
            uint32_t id = strtoul(line, &idBufferEnd, 10);
            if (id == 0) {
                fprintf(stderr, "Could not parse id of line: %s!\n", line);
                result = -1;
            }

            uint8_t flag = (uint8_t)atoi(sepFlag+1);
            size_t len = sepFlag - sepId - 1;
            if (len > UINT16_MAX) {
                fprintf(stderr, "String exceeds maximum allowed lengh: %s (%u). Skipping.\n", line, (uint16_t)UINT16_MAX);
                continue;
            }
            if (len == 0) {
                fprintf(stderr, "Zero length string encountered %u. Still writing to cliloc.\n", id);
            }

            if (fwrite(&id, sizeof(uint32_t), 1, fTmp) != 1) {
                fprintf(stderr, "Could not write id to temporary stream: %s\n", line);
                result = -1;
            }
            if (fwrite(&flag, sizeof(uint8_t), 1, fTmp) != 1) {
                fprintf(stderr, "Could not write flags to temporary stream: %s\n", line);
                result = -1;
            }
            uint16_t lenU16 = (uint16_t)len;
            if (fwrite(&lenU16, sizeof(uint16_t), 1, fTmp) != 1) {
                fprintf(stderr, "Could not write length to temporary stream: %s\n", line);
                result = -1;
            }
            if (lenU16 > 0) {
                if (fwrite((sepId+1), lenU16, 1, fTmp) != 1) {
                    fprintf(stderr, "Could not write string to temporary stream: %s\n", line);
                    result = -1;
                }
            }
        }
        if (line) {
            free(line);
        }
    }
    // open output stream
    if (result == 0) {
        fCliloc = fopen(a_output, "wb");
        if (!fCliloc) {
            fprintf(stderr, "Could not open output file!\n");
            result = -1;
        }
    }
    // copy scratch stream to output stream
    if (result == 0) {
        if (copy_content(fTmp, fCliloc) != 0) {
            fprintf(stderr, "Could not write output file!\n");
            result = -1;
        }
    }

    if (result == 0) {
        fprintf(stdout, "  success.\n");
    }

    // cleanup
    if (fCsv) {
        fclose(fCsv);
    }
    if (fTmp) {
        fclose(fTmp);
    }
    if (fCliloc) {
        fclose(fCliloc);
    }

    return result;
}

int main(int a_argc, char* a_argv[]) {
    if (a_argc != 3) {
        print_usage();
    }

    char* input = a_argv[1];
    char* output = a_argv[2];

    if (access(input, F_OK) != 0) {
        fprintf(stderr, "Input file does not exist!\n");
        return -1;
    }

    int typeFrom = get_file_type(input);
    int typeTo = get_file_type(output);
    int validFileTypes = 0;
    if (typeFrom == FILE_UNKNOWN) {
        fprintf(stderr, "Unknown input file type!\n");
        validFileTypes = -1;
    }
    if (typeTo == FILE_UNKNOWN) {
        fprintf(stderr, "Unknown output file type!\n");
        validFileTypes = -1;
    }

    if (typeFrom == FILE_CLILOC && typeTo == FILE_CSV) {
        return cliloc_to_csv(input, output);
    } else if (typeFrom == FILE_CSV && typeTo == FILE_CLILOC) {
        return csv_to_cliloc(input, output);
    } else {
        fprintf(stderr, "Can only convert from cliloc to csv and csv to cliloc!\n");
        return -1;
    }
}
