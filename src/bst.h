/**
 * @file bst.h
 * @brief header file for the AVL tree implementation
 *
 * Project: Implementace překladače imperativního jazyka IFJ2026
 *
 * @author Arkhyp Samoilenko (xsamoia00)
 *
 */

#ifndef BST_H
#define BST_H

typedef struct nodeData nodeData;
typedef void (*dataEraser)(nodeData* data);

typedef struct bstNode bstNode;

/** Status code for bst */
typedef enum
{
    BST_INSERTED = 0, /**< new element was added successfully*/
    BST_ERROR = 1,    /**< internal error: failed malloc, invalid arguments */
    BST_EXISTS = 2,   /**< key already present in tree */
} bstStatus;

/**
 * @brief Initialization function for AVL tree
 */
void bstInit(bstNode** root);


/**
 * @brief Inserts a value into the tree and balances the tree if needed
 */
bstStatus bstInsert(bstNode** root, const char* key, nodeData* data);


/**
 * @brief Searches for a certain value in given tree
 */
nodeData* bstSearch(bstNode* root, const char* key);


/**
 * @brief Deletes given tree and cleans all the memory
 */
void bstDestroy(bstNode** root, dataEraser eraseDataFunc);

#endif