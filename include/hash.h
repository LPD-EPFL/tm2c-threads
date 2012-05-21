#infndef _HASH_H
#define _HASH_H

#define hash hast_tw

/* Thomas Wang's 32 bit Mix Function */
INLINED unsigned int 
hash_tw(unsigned int key)
{
  key += ~(key << 15);
  key ^=  (key >> 10);
  key +=  (key << 3);
  key ^=  (key >> 6);
  key += ~(key << 11);
  key ^=  (key >> 16);
  return key;
}



#endif /* _HASH_H */
