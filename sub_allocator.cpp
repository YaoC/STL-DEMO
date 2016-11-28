//模拟SGI STL中的sub-allocator

#include <iostream>
using namespace std;

enum{__ALIGN=8}; //小型区块的上调边界
enum{__MAX_BYTES=128}; //小型区块的上界
enum{__NFREELISTS=__MAX_BYTES/__ALIGN}; //free-list 个数

class __default_alloc_template{
private:
    static size_t ROUND_UP(size_t bytes){
        return (((bytes)+__ALIGN-1)& ~(__ALIGN-1));
    }
private:
    union obj{
        union obj * free_list_link;
        char client_data[1];
    };
private:
    static obj * free_list[__NFREELISTS];
    static size_t FREELIST_INDEX(size_t bytes){
        return (((bytes)+__ALIGN-1)/__ALIGN-1);
    }
static void * refill(size_t n);
static char * chunk_alloc(size_t size,int &obj);

static char * start_free;
static char * end_free;
static char * heap_size;

public:
    static void * allocate(size_t n);
    static void   deallocate(void *p,size_t n);
    static void * reallocate(void *p,size_t old_sz, size_t new_sz);
};

//static data member 的定义和初值设定
char * __default_alloc_template::start_free=0;
char * __default_alloc_template::end_free=0;
char * __default_alloc_template::heap_size=0;

__default_alloc_template::obj * __default_alloc_template::free_list[__NFREELISTS]=
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//空间配置函数 allocate
void * __default_alloc_template::allocate(size_t n){
    obj *my_free_list;
    obj * result;
    if(n>(size_t) __MAX_BYTES){
        return;
    }
    my_free_list = free_list + FREELIST_INDEX(n);
    result = *my_free_list;
    if(result==0){
        void *r = refill(ROUND_UP(n));
        return r;
    }
    *my_free_list = result->free_list_link;
    return (result);
}

void __default_alloc_template::deallocate(void *p,size_t n){
    obj *q = (obj *)p;
    obj *my_free_list;
    if(n>(size_t) __MAX_BYTES){
        return;
    }
    my_free_list = free_list+FREELIST_INDEX(n);
    q->free_list_link=*my_free_list;
    *my_free_list = q;
}

void * __default_alloc_template::refill(size_t n){
    int nobjs = 20;
    char * chunk = chunk_alloc(n,nobjs);
    obj *my_free_list;
    obj * result;
    obj * current_obj, * next_obj;
    int i;
    if(1==nobjs) return (chunk);
    my_free_list = free_list+FREELIST_INDEX(n);
    result = (onj*) chunk;
    *my_free_list=next_obj=(obj*) (chunk+n);
    for(i=1;;i++){
        current_obj = next_obj;
        next_obj = (obj *)((char *) next_obj+n);
        if(nobjs-1==i){
            current_obj->free_list_link=0;
            break;
        }else{
            current_obj->free_list_link=next_obj;
        }
    }
    return (result);
}
