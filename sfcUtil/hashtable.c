
/*
 * hashtable.c
 *
 * (C) Copyright IBM Corp. 2005
 *
 * THIS FILE IS PROVIDED UNDER THE TERMS OF THE ECLIPSE PUBLIC LICENSE
 * ("AGREEMENT"). ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS FILE
 * CONSTITUTES RECIPIENTS ACCEPTANCE OF THE AGREEMENT.
 *
 * You can obtain a current copy of the Eclipse Public License from
 * http://www.opensource.org/licenses/eclipse-1.0.php
 *
 * Author:        Keith Pomakis <pomaki@pobox.xom>
 * Contributions: Adrian Schuur <schuur@de.ibm.com>
 *
 * Description:
 *
 * hashtable implementation.
 *
 */

/*--------------------------------------------------------------------------*\
 *                   -----===== HashTable =====-----
 *
 * Author: Keith Pomakis (pomakis@pobox.com)
 * Date:   August, 1998
 *
\*--------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "hashtable.h"
#include "utilft.h"

#define NEW(x) ((x *) malloc(sizeof(x)))

static int      pointercmp(const void *pointer1, const void *pointer2);
static unsigned long pointerHashFunction(const void *pointer);
static int      isProbablePrime(long number);
static long     calculateIdealNumOfBuckets(HashTable * hashTable);

static void    *HashTableGet(const HashTable * hashTable, const void *key);
static void     HashTableRehash(HashTable * hashTable, long numOfBuckets);

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableCreate() - creates a new HashTable
 *  DESCRIPTION:
 *      Creates a new HashTable.  When finished with this HashTable, it
 *      should be explicitly destroyed by calling the HashTableDestroy()
 *      function.
 *  EFFICIENCY:
 *      O(1)
 *  ARGUMENTS:
 *      numOfBuckets - the number of buckets to start the HashTable out with.
 *                     Must be greater than zero, and should be prime.
 *                     Ideally, the number of buckets should between 1/5
 *                     and 1 times the expected number of elements in the
 *                     HashTable.  Values much more or less than this will
 *                     result in wasted memory or decreased performance
 *                     respectively.  The number of buckets in a HashTable
 *                     can be re-calculated to an appropriate number by
 *                     calling the HashTableRehash() function once the
 *                     HashTable has been populated.  The number of buckets
 *                     in a HashTable may also be re-calculated
 *                     automatically if the ratio of elements to buckets
 *                     passes the thresholds set by HashTableSetIdealRatio().
 *  RETURNS:
 *      HashTable    - a new Hashtable, or NULL on error
\*--------------------------------------------------------------------------*/

void           *
HashTableCreate(long numOfBuckets)
{
  HashTable      *hashTable;
  int             i;

  assert(numOfBuckets > 0);

  hashTable = (HashTable *) malloc(sizeof(HashTable));
  if (hashTable == NULL)
    return NULL;

  hashTable->bucketArray = (KeyValuePair **)
      malloc(numOfBuckets * sizeof(KeyValuePair *));
  if (hashTable->bucketArray == NULL) {
    free(hashTable);
    return NULL;
  }

  hashTable->numOfBuckets = numOfBuckets;
  hashTable->numOfElements = 0;

  for (i = 0; i < numOfBuckets; i++)
    hashTable->bucketArray[i] = NULL;

  hashTable->idealRatio = 3.0;
  hashTable->lowerRehashThreshold = 0.0;
  hashTable->upperRehashThreshold = 15.0;

  hashTable->keycmp = pointercmp;
  hashTable->valuecmp = pointercmp;
  hashTable->hashFunction = pointerHashFunction;
  hashTable->keyDeallocator = NULL;
  hashTable->valueDeallocator = NULL;

  return hashTable;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableDestroy() - destroys an existing HashTable
 *  DESCRIPTION:
 *      Destroys an existing HashTable.
 *  EFFICIENCY:
 *      O(n)
 *  ARGUMENTS:
 *      hashTable    - the HashTable to destroy
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

void mcs ()
{
  return;
}

static void
HashTableDestroy(HashTable * hashTable)
{
  int             i;

  for (i = 0; i < hashTable->numOfBuckets; i++) {
    KeyValuePair   *pair = hashTable->bucketArray[i];
    while (pair != NULL) {
      KeyValuePair   *nextPair = pair->next;
      if (hashTable->keyDeallocator != NULL)
        hashTable->keyDeallocator((void *) pair->key);
      if (hashTable->valueDeallocator != NULL)
        hashTable->valueDeallocator(pair->value);
      free(pair);
      pair = nextPair;
    }
  }

  free(hashTable->bucketArray);
  free(hashTable);
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableContainsKey() - checks the existence of a key in a HashTable
 *  DESCRIPTION:
 *      Determines whether or not the specified HashTable contains the
 *      specified key.  Uses the comparison function specified by
 *      HashTableSetKeyComparisonFunction().
 *  EFFICIENCY:
 *      O(1), assuming a good hash function and element-to-bucket ratio
 *  ARGUMENTS:
 *      hashTable    - the HashTable to search
 *      key          - the key to search for
 *  RETURNS:
 *      bool         - whether or not the specified HashTable contains the
 *                     specified key.
\*--------------------------------------------------------------------------*/

static int
HashTableContainsKey(const HashTable * hashTable, const void *key)
{
  return (HashTableGet(hashTable, key) != NULL);
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableContainsValue()
 *                         - checks the existence of a value in a HashTable
 *  DESCRIPTION:
 *      Determines whether or not the specified HashTable contains the
 *      specified value.  Unlike HashTableContainsKey(), this function is
 *      not very efficient, since it has to scan linearly looking for a
 *      match.  Uses the comparison function specified by
 *      HashTableSetValueComparisonFunction().
 *  EFFICIENCY:
 *      O(n)
 *  ARGUMENTS:
 *      hashTable    - the HashTable to search
 *      value        - the value to search for
 *  RETURNS:
 *      bool         - whether or not the specified HashTable contains the
 *                     specified value.
\*--------------------------------------------------------------------------*/

static int
HashTableContainsValue(const HashTable * hashTable, const void *value)
{
  int             i;

  for (i = 0; i < hashTable->numOfBuckets; i++) {
    KeyValuePair   *pair = hashTable->bucketArray[i];
    while (pair != NULL) {
      if (hashTable->valuecmp(value, pair->value) == 0)
        return 1;
      pair = pair->next;
    }
  }

  return 0;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTablePut() - adds a key/value pair to a HashTable
 *  DESCRIPTION:
 *      Adds the specified key/value pair to the specified HashTable.  If
 *      the key already exists in the HashTable (determined by the comparison
 *      function specified by HashTableSetKeyComparisonFunction()), its value
 *      is replaced by the new value.  May trigger an auto-rehash (see
 *      HashTableSetIdealRatio()).  It is illegal to specify NULL as the
 *      key or value.
 *  EFFICIENCY:
 *      O(1), assuming a good hash function and element-to-bucket ratio
 *  ARGUMENTS:
 *      hashTable    - the HashTable to add to
 *      key          - the key to add or whose value to replace
 *      value        - the value associated with the key
 *  RETURNS:
 *      err          - 0 if successful, -1 if an error was encountered
\*--------------------------------------------------------------------------*/

static int
HashTablePut(HashTable * hashTable, const void *key, void *value)
{
  long            hashValue;
  KeyValuePair   *pair;

  assert(key != NULL);
  assert(value != NULL);

  hashValue = hashTable->hashFunction(key) % hashTable->numOfBuckets;
  pair = hashTable->bucketArray[hashValue];

  while (pair != NULL && hashTable->keycmp(key, pair->key) != 0)
    pair = pair->next;

  if (pair) {
    if (pair->key != key) {
      if (hashTable->keyDeallocator != NULL)
        hashTable->keyDeallocator((void *) pair->key);
      pair->key = key;
    }
    if (pair->value != value) {
      if (hashTable->valueDeallocator != NULL)
        hashTable->valueDeallocator(pair->value);
      pair->value = value;
    }
  } else {
    KeyValuePair   *newPair =
        (KeyValuePair *) malloc(sizeof(KeyValuePair));
    if (newPair == NULL) {
      return -1;
    } else {
      newPair->key = key;
      newPair->value = value;
      newPair->next = hashTable->bucketArray[hashValue];
      hashTable->bucketArray[hashValue] = newPair;
      hashTable->numOfElements++;

      if (hashTable->upperRehashThreshold > hashTable->idealRatio) {
        float           elementToBucketRatio =
            (float) hashTable->numOfElements /
            (float) hashTable->numOfBuckets;
        if (elementToBucketRatio > hashTable->upperRehashThreshold)
          HashTableRehash(hashTable, 0);
      }
    }
  }

  return 0;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableGet() - retrieves the value of a key in a HashTable
 *  DESCRIPTION:
 *      Retrieves the value of the specified key in the specified HashTable.
 *      Uses the comparison function specified by
 *      HashTableSetKeyComparisonFunction().
 *  EFFICIENCY:
 *      O(1), assuming a good hash function and element-to-bucket ratio
 *  ARGUMENTS:
 *      hashTable    - the HashTable to search
 *      key          - the key whose value is desired
 *  RETURNS:
 *      void *       - the value of the specified key, or NULL if the key
 *                     doesn't exist in the HashTable
\*--------------------------------------------------------------------------*/

static void    *
HashTableGet(const HashTable * hashTable, const void *key)
{
  long            hashValue =
      hashTable->hashFunction(key) % hashTable->numOfBuckets;
  KeyValuePair   *pair = hashTable->bucketArray[hashValue];

  while (pair != NULL && hashTable->keycmp(key, pair->key) != 0)
    pair = pair->next;

  return (pair == NULL) ? NULL : pair->value;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableRemove() - removes a key/value pair from a HashTable
 *  DESCRIPTION:
 *      Removes the key/value pair identified by the specified key from the
 *      specified HashTable if the key exists in the HashTable.  May trigger
 *      an auto-rehash (see HashTableSetIdealRatio()).
 *  EFFICIENCY:
 *      O(1), assuming a good hash function and element-to-bucket ratio
 *  ARGUMENTS:
 *      hashTable    - the HashTable to remove the key/value pair from
 *      key          - the key specifying the key/value pair to be removed
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableRemove(HashTable * hashTable, const void *key)
{
  long            hashValue =
      hashTable->hashFunction(key) % hashTable->numOfBuckets;
  KeyValuePair   *pair = hashTable->bucketArray[hashValue];
  KeyValuePair   *previousPair = NULL;

  while (pair != NULL && hashTable->keycmp(key, pair->key) != 0) {
    previousPair = pair;
    pair = pair->next;
  }

  if (pair != NULL) {
    if (hashTable->keyDeallocator != NULL)
      hashTable->keyDeallocator((void *) pair->key);
    if (hashTable->valueDeallocator != NULL)
      hashTable->valueDeallocator(pair->value);
    if (previousPair != NULL)
      previousPair->next = pair->next;
    else
      hashTable->bucketArray[hashValue] = pair->next;
    free(pair);
    hashTable->numOfElements--;

    if (hashTable->lowerRehashThreshold > 0.0) {
      float           elementToBucketRatio =
          (float) hashTable->numOfElements /
          (float) hashTable->numOfBuckets;
      if (elementToBucketRatio < hashTable->lowerRehashThreshold)
        HashTableRehash(hashTable, 0);
    }
  }
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableRemoveAll() - removes all key/value pairs from a HashTable
 *  DESCRIPTION:
 *      Removes all key/value pairs from the specified HashTable.  May trigger
 *      an auto-rehash (see HashTableSetIdealRatio()).
 *  EFFICIENCY:
 *      O(n)
 *  ARGUMENTS:
 *      hashTable    - the HashTable to remove all key/value pairs from
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableRemoveAll(HashTable * hashTable)
{
  int             i;

  for (i = 0; i < hashTable->numOfBuckets; i++) {
    KeyValuePair   *pair = hashTable->bucketArray[i];
    while (pair != NULL) {
      KeyValuePair   *nextPair = pair->next;
      if (hashTable->keyDeallocator != NULL)
        hashTable->keyDeallocator((void *) pair->key);
      if (hashTable->valueDeallocator != NULL)
        hashTable->valueDeallocator(pair->value);
      free(pair);
      pair = nextPair;
    }
    hashTable->bucketArray[i] = NULL;
  }

  hashTable->numOfElements = 0;
  HashTableRehash(hashTable, 5);
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableIsEmpty() - determines if a HashTable is empty
 *  DESCRIPTION:
 *      Determines whether or not the specified HashTable contains any
 *      key/value pairs.
 *  EFFICIENCY:
 *      O(1)
 *  ARGUMENTS:
 *      hashTable    - the HashTable to check
 *  RETURNS:
 *      bool         - whether or not the specified HashTable contains any
 *                     key/value pairs
\*--------------------------------------------------------------------------*/

static int
HashTableIsEmpty(const HashTable * hashTable)
{
  return (hashTable->numOfElements == 0);
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSize() - returns the number of elements in a HashTable
 *  DESCRIPTION:
 *      Returns the number of key/value pairs that are present in the
 *      specified HashTable.
 *  EFFICIENCY:
 *      O(1)
 *  ARGUMENTS:
 *      hashTable    - the HashTable whose size is requested
 *  RETURNS:
 *      long         - the number of key/value pairs that are present in
 *                     the specified HashTable
\*--------------------------------------------------------------------------*/

static long
HashTableSize(const HashTable * hashTable)
{
  return hashTable->numOfElements;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableGetNumBuckets() - returns the number of buckets in a HashTable
 *  DESCRIPTION:
 *      Returns the number of buckets that are in the specified HashTable.
 *      This may change dynamically throughout the life of a HashTable if
 *      automatic or manual rehashing is performed.
 *  EFFICIENCY:
 *      O(1)
 *  ARGUMENTS:
 *      hashTable    - the HashTable whose number of buckets is requested
 *  RETURNS:
 *      long         - the number of buckets that are in the specified
 *                     HashTable
\*--------------------------------------------------------------------------*/

static long
HashTableGetNumBuckets(const HashTable * hashTable)
{
  return hashTable->numOfBuckets;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSetKeyComparisonFunction()
 *              - specifies the function used to compare keys in a HashTable
 *  DESCRIPTION:
 *      Specifies the function used to compare keys in the specified
 *      HashTable.  The specified function should return zero if the two
 *      keys are considered equal, and non-zero otherwise.  The default
 *      function is one that simply compares pointers.
 *  ARGUMENTS:
 *      hashTable    - the HashTable whose key comparison function is being
 *                     specified
 *      keycmp       - a function which returns zero if the two arguments
 *                     passed to it are considered "equal" keys and non-zero
 *                     otherwise
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableSetKeyComparisonFunction(HashTable * hashTable,
                                  int (*keycmp) (const void
                                                 *key1, const void *key2))
{
  assert(keycmp != NULL);
  hashTable->keycmp = keycmp;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSetValueComparisonFunction()
 *              - specifies the function used to compare values in a HashTable
 *  DESCRIPTION:
 *      Specifies the function used to compare values in the specified
 *      HashTable.  The specified function should return zero if the two
 *      values are considered equal, and non-zero otherwise.  The default
 *      function is one that simply compares pointers.
 *  ARGUMENTS:
 *      hashTable    - the HashTable whose value comparison function is being
 *                     specified
 *      valuecmp     - a function which returns zero if the two arguments
 *                     passed to it are considered "equal" values and non-zero
 *                     otherwise
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableSetValueComparisonFunction(HashTable * hashTable,
                                    int (*valuecmp) (const void
                                                     *value1,
                                                     const void *value2))
{
  assert(valuecmp != NULL);
  hashTable->valuecmp = valuecmp;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSetHashFunction()
 *              - specifies the hash function used by a HashTable
 *  DESCRIPTION:
 *      Specifies the function used to determine the hash value for a key
 *      in the specified HashTable (before modulation).  An ideal hash
 *      function is one which is easy to compute and approximates a
 *      "random" function.  The default function is one that works
 *      relatively well for pointers.  If the HashTable keys are to be
 *      strings (which is probably the case), then this default function
 *      will not suffice, in which case consider using the provided
 *      HashTableStringHashFunction() function.
 *  ARGUMENTS:
 *      hashTable    - the HashTable whose hash function is being specified
 *      hashFunction - a function which returns an appropriate hash code
 *                     for a given key
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableSetHashFunction(HashTable * hashTable,
                         unsigned long (*hashFunction) (const void *key))
{
  assert(hashFunction != NULL);
  hashTable->hashFunction = hashFunction;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableRehash() - reorganizes a HashTable to be more efficient
 *  DESCRIPTION:
 *      Reorganizes a HashTable to be more efficient.  If a number of
 *      buckets is specified, the HashTable is rehashed to that number of
 *      buckets.  If 0 is specified, the HashTable is rehashed to a number
 *      of buckets which is automatically calculated to be a prime number
 *      that achieves (as closely as possible) the ideal element-to-bucket
 *      ratio specified by the HashTableSetIdealRatio() function.
 *  EFFICIENCY:
 *      O(n)
 *  ARGUMENTS:
 *      hashTable    - the HashTable to be reorganized
 *      numOfBuckets - the number of buckets to rehash the HashTable to.
 *                     Should be prime.  Ideally, the number of buckets
 *                     should be between 1/5 and 1 times the expected
 *                     number of elements in the HashTable.  Values much
 *                     more or less than this will result in wasted memory
 *                     or decreased performance respectively.  If 0 is
 *                     specified, an appropriate number of buckets is
 *                     automatically calculated.
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableRehash(HashTable * hashTable, long numOfBuckets)
{
  KeyValuePair  **newBucketArray;
  int             i;

  assert(numOfBuckets >= 0);
  if (numOfBuckets == 0)
    numOfBuckets = calculateIdealNumOfBuckets(hashTable);

  if (numOfBuckets == hashTable->numOfBuckets)
    return;                     /* already the right size! */

  newBucketArray = (KeyValuePair **)
      malloc(numOfBuckets * sizeof(KeyValuePair *));
  if (newBucketArray == NULL) {
    /*
     * Couldn't allocate memory for the new array.  This isn't a fatal
     * error; we just can't perform the rehash. 
     */
    return;
  }

  for (i = 0; i < numOfBuckets; i++)
    newBucketArray[i] = NULL;

  for (i = 0; i < hashTable->numOfBuckets; i++) {
    KeyValuePair   *pair = hashTable->bucketArray[i];
    while (pair != NULL) {
      KeyValuePair   *nextPair = pair->next;
      long            hashValue =
          hashTable->hashFunction(pair->key) % numOfBuckets;
      pair->next = newBucketArray[hashValue];
      newBucketArray[hashValue] = pair;
      pair = nextPair;
    }
  }

  free(hashTable->bucketArray);
  hashTable->bucketArray = newBucketArray;
  hashTable->numOfBuckets = numOfBuckets;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSetIdealRatio()
 *              - sets the ideal element-to-bucket ratio of a HashTable
 *  DESCRIPTION:
 *      Sets the ideal element-to-bucket ratio, as well as the lower and
 *      upper auto-rehash thresholds, of the specified HashTable.  Note
 *      that this function doesn't actually perform a rehash.
 *
 *      The default values for these properties are 3.0, 0.0 and 15.0
 *      respectively.  This is likely fine for most situations, so there
 *      is probably no need to call this function.
 *  ARGUMENTS:
 *      hashTable    - a HashTable
 *      idealRatio   - the ideal element-to-bucket ratio.  When a rehash
 *                     occurs (either manually via a call to the
 *                     HashTableRehash() function or automatically due the
 *                     the triggering of one of the thresholds below), the
 *                     number of buckets in the HashTable will be
 *                     recalculated to be a prime number that achieves (as
 *                     closely as possible) this ideal ratio.  Must be a
 *                     positive number.
 *      lowerRehashThreshold
 *                   - the element-to-bucket ratio that is considered
 *                     unacceptably low (i.e., too few elements per bucket).
 *                     If the actual ratio falls below this number, a
 *                     rehash will automatically be performed.  Must be
 *                     lower than the value of idealRatio.  If no ratio
 *                     is considered unacceptably low, a value of 0.0 can
 *                     be specified.
 *      upperRehashThreshold
 *                   - the element-to-bucket ratio that is considered
 *                     unacceptably high (i.e., too many elements per bucket).
 *                     If the actual ratio rises above this number, a
 *                     rehash will automatically be performed.  Must be
 *                     higher than idealRatio.  However, if no ratio
 *                     is considered unacceptably high, a value of 0.0 can
 *                     be specified.
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/
/*
 * static void HashTableSetIdealRatio(HashTable *hashTable, float
 * idealRatio, float lowerRehashThreshold, float upperRehashThreshold) {
 * assert(idealRatio > 0.0); assert(lowerRehashThreshold < idealRatio);
 * assert(upperRehashThreshold == 0.0 || upperRehashThreshold >
 * idealRatio);
 * 
 * hashTable->idealRatio = idealRatio; hashTable->lowerRehashThreshold =
 * lowerRehashThreshold; hashTable->upperRehashThreshold =
 * upperRehashThreshold; } 
 */
/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableSetDeallocationFunctions()
 *              - sets the key and value deallocation functions of a HashTable
 *  DESCRIPTION:
 *      Sets the key and value deallocation functions of the specified
 *      HashTable.  This determines what happens to a key or a value when it
 *      is removed from the HashTable.  If the deallocation function is NULL
 *      (the default if this function is never called), its reference is
 *      simply dropped and it is up to the calling program to perform the
 *      proper memory management.  If the deallocation function is non-NULL,
 *      it is called to free the memory used by the object.  E.g., for simple
 *      objects, an appropriate deallocation function may be free().
 *
 *      This affects the behaviour of the HashTableDestroy(), HashTablePut(),
 *      HashTableRemove() and HashTableRemoveAll() functions.
 *  ARGUMENTS:
 *      hashTable    - a HashTable
 *      keyDeallocator
 *                   - if non-NULL, the function to be called when a key is
 *                     removed from the HashTable.
 *      valueDeallocator
 *                   - if non-NULL, the function to be called when a value is
 *                     removed from the HashTable.
 *  RETURNS:
 *      <nothing>
\*--------------------------------------------------------------------------*/

static void
HashTableSetDeallocationFunctions(HashTable * hashTable,
                                  void (*keyDeallocator) (void
                                                          *key),
                                  void (*valueDeallocator)
                                                  (void *value))
{
  hashTable->keyDeallocator = keyDeallocator;
  hashTable->valueDeallocator = valueDeallocator;
}

/*--------------------------------------------------------------------------*\
 *  NAME:
 *      HashTableStringHashFunction() - a good hash function for strings
 *  DESCRIPTION:
 *      A hash function that is appropriate for hashing strings.  Note that
 *      this is not the default hash function.  To make it the default hash
 *      function, call HashTableSetHashFunction(hashTable,
 *      HashTableStringHashFunction).
 *  ARGUMENTS:
 *      key    - the key to be hashed
 *  RETURNS:
 *      unsigned long - the unmodulated hash value of the key
\*--------------------------------------------------------------------------*/
/*
 * static unsigned long HashTableStringHashFunction(const void *key) {
 * const unsigned char *str = (const unsigned char *) key; unsigned long
 * hashValue = 0; int i;
 * 
 * for (i=0; str[i] != '\0'; i++) hashValue = hashValue * 37 + str[i];
 * 
 * return hashValue; } 
 */
static int
pointercmp(const void *pointer1, const void *pointer2)
{
  return (pointer1 != pointer2);
}

static unsigned long
pointerHashFunction(const void *pointer)
{
  return ((unsigned long) pointer) >> 4;
}

static int
isProbablePrime(long oddNumber)
{
  long            i;

  for (i = 3; i < 51; i += 2)
    if (oddNumber == i)
      return 1;
    else if (oddNumber % i == 0)
      return 0;

  return 1;                     /* maybe */
}

static long
calculateIdealNumOfBuckets(HashTable * hashTable)
{
  long            idealNumOfBuckets =
      hashTable->numOfElements / hashTable->idealRatio;
  if (idealNumOfBuckets < 5)
    idealNumOfBuckets = 5;
  else
    idealNumOfBuckets |= 0x01;  /* make it an odd number */
  while (!isProbablePrime(idealNumOfBuckets))
    idealNumOfBuckets += 2;
  return idealNumOfBuckets;
}

static UtilHashTable *
NotSupported(UtilHashTable * ht)
{
  return NULL;
}

static void
hashTableDestroy(UtilHashTable * ht)
{
  HashTableDestroy((HashTable *) ht->hdl);
  free(ht);
}

static void
hashTableRemoveAll(UtilHashTable * ht)
{
  HashTableRemoveAll((HashTable *) ht->hdl);
}

static int
hashTableContainsKey(const UtilHashTable * ht, const void *key)
{
  return HashTableContainsKey((HashTable *) ht->hdl, key);
}

static int
hashTableContainsValue(const UtilHashTable * ht, const void *val)
{
  return HashTableContainsValue((HashTable *) ht->hdl, val);
}

static int
hashTablePut(UtilHashTable * ht, const void *key, void *val)
{
  return HashTablePut((HashTable *) ht->hdl, key, val);
}

static void    *
hashTableGet(const UtilHashTable * ht, const void *key)
{
  return HashTableGet((HashTable *) ht->hdl, key);
}

static void
hashTableRemove(UtilHashTable * ht, const void *key)
{
  HashTableRemove((HashTable *) ht->hdl, key);
}

static int
hashTableIsEmpty(const UtilHashTable * ht)
{
  return HashTableIsEmpty((HashTable *) ht->hdl);
}

static int
hashTableSize(const UtilHashTable * ht)
{
  return HashTableSize((HashTable *) ht->hdl);
}

static int
hashTableGetNumBuckets(const UtilHashTable * ht)
{
  return HashTableGetNumBuckets((HashTable *) ht->hdl);
}

static void
hashTableRehash(UtilHashTable * ht, int buckets)
{
  HashTableRehash((HashTable *) ht->hdl, buckets);
}

static HashTableIterator *
hashTableGetFirst(UtilHashTable * ht, void **key, void **val)
{
  HashTable      *t = (HashTable *) ht->hdl;
  HashTableIterator *iter = NEW(HashTableIterator);
  for (iter->bucket = 0; iter->bucket < t->numOfBuckets; iter->bucket++) {
    iter->pair = t->bucketArray[iter->bucket];
    if (iter->pair != NULL) {
      *key = (void *) iter->pair->key;
      *val = iter->pair->value;
      return iter;
    }
  }
  free(iter);
  return NULL;
}

static HashTableIterator *
hashTableGetNext(UtilHashTable * ht,
                 HashTableIterator * iter, void **key, void **val)
{
  HashTable      *t = (HashTable *) ht->hdl;
  iter->pair = iter->pair->next;
  while (iter->bucket < t->numOfBuckets) {
    if (iter->pair == NULL) {
      if (iter->bucket + 1 < t->numOfBuckets)
        iter->pair = t->bucketArray[++iter->bucket];
      else
        break;
      continue;
    }
    *key = (void *) iter->pair->key;
    *val = iter->pair->value;
    return iter;
  }
  free(iter);
  return NULL;
}

static void
hashTableSetKeyComparisonFunction(UtilHashTable * ht,
                                  int (*keycomp) (const void
                                                  *k1, const void *k2))
{
  HashTable      *t = (HashTable *) ht->hdl;
  HashTableSetKeyComparisonFunction(t, keycomp);
}

static void
hashTableSetValueComparisonFunction(UtilHashTable * ht,
                                    int (*valcomp) (const void
                                                    *v1, const void *v2))
{
  HashTable      *t = (HashTable *) ht->hdl;
  HashTableSetValueComparisonFunction(t, valcomp);
}

static void
hashTableSetHashFunction(UtilHashTable * ht,
                         unsigned long (*hashFunction) (const void *key))
{
  HashTable      *t = (HashTable *) ht->hdl;
  HashTableSetHashFunction(t, hashFunction);
}

static void
hashTableSetDeallocationFunctions(UtilHashTable * ht,
                                  void (*keyRelease) (void
                                                      *key),
                                  void (*valueRelease) (void *value))
{
  HashTable      *t = (HashTable *) ht->hdl;
  HashTableSetDeallocationFunctions(t, keyRelease, valueRelease);
}

static Util_HashTable_FT ift = {
  1,
  hashTableDestroy,             // release
  NotSupported,                 // clone
  hashTableRemoveAll,           // clear
  hashTableContainsKey,         // containsKey
  hashTableContainsValue,       // containsValue
  hashTablePut,                 // put
  hashTableGet,                 // get
  hashTableRemove,              // remove
  hashTableIsEmpty,             // isEmpty
  hashTableSize,                // size
  hashTableGetNumBuckets,       // buckets
  hashTableRehash,              // rehash

  hashTableGetFirst,            // getFirst
  hashTableGetNext,             // getNext

  hashTableSetKeyComparisonFunction,    // setKeyCmpFunction
  hashTableSetValueComparisonFunction,  // setValueCmpFunction
  hashTableSetHashFunction,     // setHashFunction
  hashTableSetDeallocationFunctions,    // setReleaseFunctions
};

Util_HashTable_FT *UtilHashTableFT = &ift;
/* MODELINES */
/* DO NOT EDIT BELOW THIS COMMENT */
/* Modelines are added by 'make pretty' */
/* -*- Mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vi:set ts=2 sts=2 sw=2 expandtab: */