// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define bid(dev, blockno) ((dev*131 + blockno) % NBUCKET)
extern uint ticks;

struct {
  struct spinlock lock[NBUCKET] ;
  struct buf buf[NBUF];
  struct buf hashbucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = bid(dev, blockno);
  acquire(&bcache.lock[id]);

  // Is the block already cached?
  for(b = bcache.hashbucket[id].next; b != &bcache.hashbucket[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  int nid = (id + 1) % NBUCKET;
  while(nid != id){
    int minticks = 1e9;
    struct buf* targetb;
    acquire(&bcache.lock[nid]);
    for(b = bcache.hashbucket[nid].next; b != &bcache.hashbucket[nid]; b = b->next){
      if(b->refcnt == 0) {
        if(b->timestamp <= minticks) minticks = b->timestamp;
        targetb = b;
      }
    }
    if(minticks != 1e9){ // buffer found
      b = targetb;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      // break from original hashbucket
      b->next->prev = b->prev;
      b->prev->next = b->next;

      // insert into current hashbucket
      b->next = bcache.hashbucket[id].next;
      b->prev = &bcache.hashbucket[id];
      b->next->prev = b;
      b->prev->next = b;
      
      release(&bcache.lock[nid]);
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.lock[nid]);
    nid = (nid + 1) % NBUCKET;
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  //int id = bid(b->dev, b->blockno);
  //acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    /*
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
    */
    b->timestamp = ticks;
  }
  
  //release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = bid(b->dev, b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = bid(b->dev, b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


