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
    //给定大小bytes,返回bytes剩余的free_list编号
    //例如 bytes=6<=8,则属于第0个free_list
    //例如 bytes=16<=16,则属于第1个free_list
    static size_t FREELIST_INDEX(size_t bytes){
        return (((bytes)+__ALIGN-1)/__ALIGN-1);
    }
static void * refill(size_t n);
static char * chunk_alloc(size_t size,int &obj);

static char * start_free;
static char * end_free;
static size_t heap_size;

public:
    static void * allocate(size_t n);
    static void   deallocate(void *p,size_t n);
    static void * reallocate(void *p,size_t old_sz, size_t new_sz);
};

//static data member 的定义和初值设定
char * __default_alloc_template::start_free=0;
char * __default_alloc_template::end_free=0;
size_t __default_alloc_template::heap_size=0;

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

//从内存池取出内存供free-list使用，如果内存池无内存使用，则向系统请求更多内存
//内存池：
//start_free 当前可供使用的内存起始位置
//end_free   当前可供使用的内存结束位置
//heap_size  从系统请求的内存总量
//@size_t size  请求的块大小
//@int &nobjs   请求的块数量
char * __default_alloc_template::chunk_alloc(size_t size,int &nobjs){
    char * result;  //返回给free_list的内存
    size_t total_bytes = size*nobjs;    //请求的内存总大小
    size_t bytes_left =end_free-start_free;     //当前内存池中剩余的内存大小
    if(bytes_left>=total_bytes){
        //如果内存剩余大小满足请求
        result = start_free;
        start_free+=total_bytes;
        return (result);
    }else if(bytes_left>=size){
        //如果内存剩余大小不满足请求，但剩余内存还可以提供至少一个块
        nobjs = bytes_left/size;  //内存到底还能提供多少给块
        total_bytes = size*nobjs;  //更改请求大小
        result = start_free;
        start_free+=total_bytes;
        return (result);
    }else{
        //如果剩余内存连一个块都不能满足
        size_t bytes_to_get = 2*total_bytes+ROUND_UP(heap_size>>4);  //向系统请求bytes_to_get大小的内存,这个大小和heap_size也有关，heap_size越大，请求的量也越多
        if(bytes_left>0){
            //如果还是有一点点剩余（这点剩余无法满足一个块，但也不能浪费，也许它还能被较小的块利用）
            obj * my_free_list=free_list+FREELIST_INDEX(bytes_left);
            ((obj*) start_free)->free_list_link=*my_free_list;
            *my_free_list = (obj*) start_free;
            //在这里如果剩余内存bytes_left=5，根据代码将会把它分给free_list[0],也就是每块为8的free_list
            //但5还是不够8使用，是不是留着等下次有碎片分来时合在一起凑够8？
        }
        //处理完剩余的碎片，正式向系统请求新的内存（好严谨：)）
        start_free = (char*) malloc(bytes_to_get);
        if(0==start_free){
            //然鹅请求内存并不成功（：<）
            //只能去找老乡帮忙了
            //挨家挨户问问还有没有余粮
            int i;
            obj * my_free_list, *p;
            //要问也要问比我阔的，比我穷的还看不上
            //侯捷：我们不打算尝试配置较小的区块，因为那在多进程机器上容易造成灾难
            //这是为什么呢？原子性？
            //总之就是找“尚未使用，且足够大（比我大）”的区块
            for (i=size; i <= __MAX_BYTES; i+=__ALIGN){
                my_free_list = free_list+FREELIST_INDEX(i);  //为什么要从它自己对应的链表找？是否是因为之前处理碎片时合并的碎片可能能满足一个区块的使用？
                p = *my_free_list;
                if(0!=p){
                    *my_free_list = p->free_list_link;
                    start_free = (char *)p;
                    end_free = start_free+i;
                    return (chunk_alloc(size,nobjs));
                }
            }
            end_free = 0;
            // start_free = (char*) malloc_alloc::allocate(bytes_to_get);
            cout<<"out of memory"<<endl;
        }
        heap_size += bytes_to_get;
        end_free=start_free+bytes_to_get;
        return (chunk_alloc(size,nobjs));
    }
}
