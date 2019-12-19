#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // read write open fclose readlink
#include <fcntl.h>  // io flags
#include <string.h>
#include <assert.h>

#include "include/common.h"
#include "include/object.h"

// Compile unit列表
LIST_HEAD(objects);

extern pid_t leader_pid;

// 16进制字符串转二进制
// 从low_byte转到high byte
static int hex_to_int(const char *high_byte, const char *low_byte, uint64_t *data_ptr) {
    uint64_t   data = 0;
    const char hex[] = {};

    assert(data_ptr != NULL);
    assert(low_byte >= high_byte);

    if(low_byte - high_byte > 16){
        return 1;
    }

    // 将字符串形式的数字转成二进制
    for(;low_byte >= high_byte; high_byte++){
        char curr_byte = *high_byte;
        if('0' <= curr_byte && curr_byte <= '9'){
            curr_byte -= '0';
        }else if ('a' <= curr_byte && curr_byte <= 'f'){
            curr_byte = curr_byte - 'a' + 10;
        }else if('A' <= curr_byte && curr_byte <= 'F'){
            curr_byte = curr_byte - 'A' + 10;
        }else{
            return 2;
        }
        data = (data << 4) | curr_byte;
    }
    *data_ptr = data;
    return 0;
}

// 解析range
// 返回下一个item的地址
static const char *parse_mmap_range(const char *mmap, uint64_t *start_ptr, uint64_t *end_ptr){
    size_t end_pos = 0, sp_pos = 0;
    char   *range_str;

    assert(mmap != NULL);
    assert(start_ptr != NULL);
    assert(end_ptr != NULL);

    // 找到下一个分隔符
    for(; mmap[end_pos] != ' '; end_pos++);
    
    range_str = strndup(mmap, end_pos);
    if(!range_str){
        return NULL;
    }
    
    // 找到中间的分隔符
    for(; sp_pos < end_pos && mmap[sp_pos] != '-'; sp_pos++);

    if(hex_to_int(range_str, range_str + sp_pos - 1, start_ptr)){
        printf("parse start address failed.\n");
        free(range_str);
        return NULL;
    }
    if(hex_to_int(range_str + sp_pos + 1, range_str + end_pos - 1, end_ptr)){
        printf("parse end address failed.\n");
        free(range_str);
        return NULL;
    }

    free(range_str);

    // 返回下一个非空格的位置
    return mmap + end_pos + 1;
}

// 解析内存区域的属性
static const char *parse_mmap_attr(const char *mmap, uint8_t *attr){
    size_t end_pos = 0, curr_pos = 0;
    char   *attr_str;

    assert(mmap != NULL);
    assert(attr != NULL);
    *attr = 0;

    // 找到下一个分隔符
    for(; mmap[end_pos] != ' '; end_pos++);

    // 拷贝属性字符串
    attr_str = strndup(mmap, end_pos);
    if(!attr_str){
        return NULL;
    }

    for(; curr_pos < end_pos; curr_pos++){
        switch(attr_str[curr_pos]){
            case 'r':
            *attr |= REGN_ATTR_READ;
            break;

            case 'w':
            *attr |= REGN_ATTR_WRITE;
            break;

            case 'x':
            *attr |= REGN_ATTR_EXECUTE;
            break;

            case 'p':
            *attr |= REGN_ATTR_PRIVATE;
            break;

            default:;
        }
    }

    free(attr_str);

    // 返回下一个非空格的位置
    return mmap + end_pos + 1;
}

// 解析inode
static const char *parse_mmap_inode(const char *mmap, uint32_t *inode_ptr){
    size_t end_pos = 0;
    char   *inode_str;

    assert(mmap != NULL);
    assert(inode_ptr != NULL);

    // 找到下一个分隔符X2
    for(; mmap[end_pos] != ' '; end_pos++);
    end_pos++;
    for(; mmap[end_pos] != ' '; end_pos++);
    end_pos++;

    // 找到inode起始位置
    mmap += end_pos;
    for(end_pos = 0; mmap[end_pos] != ' '; end_pos++);

    // 拷贝inode字符串
    inode_str = strndup(mmap, end_pos);
    if(!inode_str){
        return NULL;
    }
    
    // 转换成数字
    *inode_ptr = (uint32_t)atoi(inode_str);
    free(inode_str);

    // 找到下一个不是空格的字符,可能是换行符
    for(; mmap[end_pos] == ' '; end_pos++);

    return mmap + end_pos;
}

// 解析文件名
// 用完释放
static const char *parse_mmap_file(const char *mmap, char **file_ptr){
    size_t end_pos = 0;
    char   *file_str;

    assert(mmap != NULL);
    assert(file_ptr != NULL);

    // 找到换行符
    for(; mmap[end_pos] != '\n'; end_pos++);

    // 拷贝文件地址字符串
    file_str = strndup(mmap, end_pos);
    if(!file_str){
        return NULL;
    }
    *file_ptr = file_str;
    return mmap + end_pos;
}

// 解析memory map
static int object_parse_mmap(struct list_head *object_list, const char *mmap){
    int        exists = 0;
    uint64_t   start, end;
    uint8_t    attr = 0;
    uint32_t   inode = 0;
    char       *file = NULL;
    const char *mmap_end;
    struct object *obj, *curr_obj = NULL;

    assert(mmap != NULL);

    mmap_end = mmap + strlen(mmap);

    while(mmap != mmap_end){
        // 解析范围
        mmap = parse_mmap_range(mmap, &start, &end);
        if(!mmap){
            return -1;
        }

        // 解析属性
        mmap = parse_mmap_attr(mmap, &attr);
        if(!mmap){
            return -1;
        }

        //跳过offset和设备号, 解析inode
        mmap = parse_mmap_inode(mmap, &inode);
        if(!mmap){
            return -1;
        }

        // 解析文件名
        if (inode != 0) {
            mmap = parse_mmap_file(mmap, &file);
            if(!mmap){
                return -1;
            }
        }else{
            for(; *mmap != '\n'; mmap++);
            file = NULL;
        }

        // 到这个地方代表一行解析完了, mmap指向换行符
        // 指向下一个条目
        mmap++;
        
        // 如果当前区域不是可执行的,则跳过该条目
        if((attr & REGN_ATTR_EXECUTE) == 0) {
            if(file) free(file);
            file = NULL;
            continue;
        }
        
        // 跳过链表中已经存在的项
        exists = 0;
        list_for_each_entry(curr_obj, &objects, object_chain){
            if(curr_obj->text_start == start && curr_obj->text_end == end){
                exists = 1;
                break;
            }
        }
        if(exists){
            continue;
        }

        // 创建新的object,并插入链表
        obj = calloc(1, sizeof(struct object));
        if(!obj){
            return -2;
        }

        obj->text_start = start;
        obj->text_end = end;
        INIT_LIST_HEAD(&obj->breakpoint_chain);
        INIT_LIST_HEAD(&obj->object_chain);

        if(inode != 0){
            obj->file_name = file;
        }else{
            obj->file_name = "<pseudo file>";
        }

        printf("add %s .text: 0x%lX-0x%lX\n", obj->file_name, obj->text_start, obj->text_end);

        list_add_tail(&obj->object_chain, &objects);
    }
    return 0;
}

// 获取当前进程memory map
// 外部释放
static char *object_get_mmap(void) {
    unsigned char buf[256];
    char *mmap_buf = NULL, *tmp = NULL;
    size_t mmap_buf_length = 1;
    int rv = 0, fd;

    snprintf(buf, sizeof buf, "/proc/%d/maps", leader_pid);

    fd = open(buf, O_RDONLY);
    if(fd == -1){
        perror("open");
        return NULL;
    }

    do{
        rv = read(fd, buf, sizeof buf);
        if(rv == -1){
            perror("read");
            goto ERR_RET_1;
        }

        tmp = realloc(mmap_buf, mmap_buf_length + rv);
        if(!tmp){
            perror("realloc");
            goto ERR_RET_1;
        }
        mmap_buf = tmp;

        memcpy(mmap_buf + mmap_buf_length - 1, buf, rv);
        mmap_buf_length += rv;
    }while(rv > 0);

    mmap_buf[mmap_buf_length-1] = 0;

    close(fd);
    return mmap_buf;

ERR_RET_1:
    if(mmap_buf) free(mmap_buf);
    close(fd);
    return NULL;
}

// 获取当前执行二进制路径
// 外部释放
char *object_get_exe(void) {
    unsigned char buf[256];
    int rv;
    char *path_buf = NULL, *path = NULL;

    if((path_buf = malloc(4096)) == NULL){
        perror("malloc");
        return NULL;
    }

    snprintf(buf, sizeof buf, "/proc/%d/exe", leader_pid);

    if((rv = readlink(buf, path_buf, 4096)) == -1){
        perror("readlink");
        free(path_buf);
        return NULL;
    }

    path = strdup(path_buf);
    free(path_buf);

    return path;
}

// 获取指定文件的入口点
addr_t object_get_exe_entry_point(const char *name){
    FILE *fp;
    Elf32_Ehdr *ehdr;
    addr_t ep;

    assert(name != NULL);

    fp = fopen(name, "rb");
	if(fp == NULL){
		perror("fopen");
		return 0;
	}

    // Elf32和Elf64的header兼容
	ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    if(ehdr == NULL){
        perror("malloc");
        fclose(fp);
        return 0;
    }
    
	fread(ehdr, sizeof(Elf32_Ehdr), 1, fp);
    fclose(fp);

    // 检查ELF头
    if(ehdr->e_ident[0] != 0x7f || 
		ehdr->e_ident[1] != 'E' ||
		ehdr->e_ident[2] != 'L' ||
		ehdr->e_ident[3] != 'F')
	{
		fprintf(stderr, "%s is not a elf file.", name);
        free(ehdr);
		return 0;
	}
    
    ep = (addr_t)ehdr->e_entry;

    free(ehdr);

    return ep;
}

// 加载进程的object
int object_load(){
    int rv;
    // 获得进程memory map
    char *maps = object_get_mmap();
    if(!maps){
        return -1;
    }
    // 解析memory map, 构造object链表
    if((rv = object_parse_mmap(&objects, maps)) != 0){
        printf("parse profilee memory map failed! %d\n", rv);
        return -1;
    }
    return 0;
}

// 根据文件名获取object
struct object *object_get_by_file(const char *filename){
    struct object *curr_obj;

    assert(filename != NULL);

    list_for_each_entry(curr_obj, &objects, object_chain){
        if(strcmp(filename, curr_obj->file_name) == 0){
            return curr_obj;
        }
    }
    return NULL;
}

// int main(){
//     char *maps, *exe;
//     maps = object_get_mmap();
//     if(!maps){
//         return 1;
//     }
//     printf("%s\n", maps);
//     object_parse_mmap(maps);

//     exe = object_get_exe(0);
//     if(!exe){
//         free(maps);
//         return 1;
//     }
//     printf("%s\n", exe);
//     free(maps);
//     free(exe);
//     return 0;
// }