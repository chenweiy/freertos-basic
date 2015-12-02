#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>

#define hash_init 5381

uint32_t hash_djb2(const uint8_t * str, uint32_t hash) {
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;

    return hash;
}

void usage(const char * binname) {
    printf("Usage: %s [-d <dir>] [outfile]\n", binname);
    exit(-1);
}

//將此目錄下的檔案格式轉換成 hash格式，並存在outfile裡面
void processdir(DIR * dirp, const char * curpath, FILE * outfile, const char * prefix) {
    char fullpath[1024];
    char buf[16 * 1024];
    struct dirent * ent; //代表director entry
    DIR * rec_dirp; //遞迴的目錄檔
    uint32_t cur_hash = hash_djb2((const uint8_t *) curpath, hash_init); //將目前的路徑轉換成hash格式
    uint32_t size, w, hash;
    uint8_t b;
    FILE * infile;

    //reddir() 讀取此目錄,
    while ((ent = readdir(dirp))) {
        strcpy(fullpath, prefix); //prefix = test-romfs
        strcat(fullpath, "/");
        strcat(fullpath, curpath);
        strcat(fullpath, ent->d_name);

        //判斷是 目錄還是檔案
    #ifdef _WIN32
        if (GetFileAttributes(fullpath) & FILE_ATTRIBUTE_DIRECTORY) {
    #else
        if (ent->d_type == DT_DIR) {
    #endif

            if (strcmp(ent->d_name, ".") == 0) // 略過 ./ 的目錄
                continue;
            if (strcmp(ent->d_name, "..") == 0) //略過 ../
                continue;
            strcat(fullpath, "/");
            rec_dirp = opendir(fullpath); //遞迴目錄
            processdir(rec_dirp, fullpath + strlen(prefix) + 1, outfile, prefix);
            closedir(rec_dirp);
        } else {
            hash = hash_djb2((const uint8_t *) ent->d_name, cur_hash);

            infile = fopen(fullpath, "rb");
            if (!infile) {
                perror("opening input file");
                exit(-1);
            }

            //將hash過得file name 存到 outfile （共4 bytes =32 bit)
            b = (hash >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >> 24) & 0xff; fwrite(&b, 1, 1, outfile);

            fseek(infile, 0, SEEK_END);
            size = ftell(infile) + strlen(ent->d_name) + 1;
            fseek(infile, 0, SEEK_SET);
            b = (size >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >> 24) & 0xff; fwrite(&b, 1, 1, outfile);

            b = (cur_hash >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (cur_hash >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (cur_hash >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (cur_hash >> 24) & 0xff; fwrite(&b, 1, 1, outfile);

            //將實際的檔名 存進去
            fwrite(ent->d_name,strlen(ent->d_name),1,outfile);

            //存1byte 的0
            b = 0;fwrite(&b,1,1,outfile);

            //存file的全部內容
            size = size - strlen(ent->d_name) - 1;
            while (size) {
                w = size > 16 * 1024 ? 16 * 1024 : size;
                fread(buf, 1, w, infile);
                fwrite(buf, 1, w, outfile);
                size -= w;
            }
            fclose(infile);
        }
    }
}
//romfs.mk 下的執行程式：
//mkromfs -d data/test-romfs build/data/test-romfs.bin
//argv[0] = mkromfs ,也就是執行的程式名
//arg[1] = -d , arg[2] = data/test-romfs , arg[3] = build/data/test-romfs.bin
int main(int argc, char ** argv) {
    char * binname = *argv++;
    char * o;
    char * outname = NULL;
    char * dirname = ".";
    uint64_t z = 0;
    FILE * outfile;
    DIR * dirp;

    while ((o = *argv++)) {
        if (*o == '-') {
            o++;
            switch (*o) {
            case 'd':
                dirname = *argv++;
                break;
            default:
                usage(binname);
                break;
            }
        } else {
            if (outname)
                usage(binname);
            outname = o;
        }
    }

    if (!outname)
        outfile = stdout;
    else
        outfile = fopen(outname, "wb");

    if (!outfile) {
        perror("opening output file");
        exit(-1);
    }

    dirp = opendir(dirname); // open directory
    if (!dirp) {
        perror("opening directory");
        exit(-1);
    }

    processdir(dirp, "", outfile, dirname);
    fwrite(&z, 1, 8, outfile);
    if (outname)
        fclose(outfile);
    closedir(dirp);
    
    return 0;
}
