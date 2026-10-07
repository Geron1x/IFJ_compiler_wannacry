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

#include <stdbool.h>

/**
 * @brief Structure for AVL tree's node 
 */
typedef struct bstnode
{
    char* key;              /**< temp */
    void* data;             /**< temp */
    int height;             /**< temp */
    struct bstnode* left;   /**< temp */
    struct bstnode* right;  /**< temp */

} bstNode;

/**
 * @brief Initialization function for AVL tree
 */
void bstInit(bstNode **root);


/**
 * @brief Inserts a value into the tree and balances the tree if needed
 */
bool bstInsert(bstNode **root, const char* key);


/**
 * @brief Searches for a certain value in given tree
 */
bstNode* bstSearch(const bstNode *root, const char* key);


/**
 * @brief Deletes given tree and cleans all the memory
 */
void bstDestroy(bstNode **root);
#endif