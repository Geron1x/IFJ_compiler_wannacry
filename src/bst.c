/**
 * @file bst.c
 * @brief File for the AVL tree implementation
 * 
 * Project: Implementace překladače imperativního jazyka IFJ2026
 * 
 * @author Arkhyp Samoilenko (xsamoia00)
 * 
 */

#include "bst.h"
#include <string.h>
#include <stdlib.h>

static bstNode* allocateNode(const char* key, nodeData* data);
static int getHeight(const bstNode *node);
static int getMax(int a, int b);
static bool placeNode(const char* newKey, bstNode** parent, nodeData* data);
static void rotateLeft(bstNode** parent);
static void rotateRight(bstNode** parent);


void bstInit(bstNode **root)
{
    if (root != NULL)
    {
        *root = NULL;
    }
}
bool bstInsert(bstNode **root, const char* key, nodeData* data)
{
    if (root == NULL || key == NULL)
    {
        return false;
    }
    return placeNode(key, root, data);
}

static bool placeNode(const char* newKey, bstNode** parent, nodeData* data)
{
    /** check for a first insertion 
     * and last insertion to either left of right child of a previously childless parent
     */
    if(*parent == NULL)
    {
        *parent = allocateNode(newKey, data);
        return true;
    }
    /** comparing both keys using strcmp */
    int cmpResult = strcmp(newKey, (*parent)->key);
    /** variable to save the return value of the recursive function.
     *  if no node was added, ignore everything after and deliver false to the caller (bstInsert) 
    */
    bool wasPlaced = false;
    /** case 1: newKey is less (in terms of ASCII table) than parent's key */
    if(cmpResult < 0)
    {
        wasPlaced = placeNode(newKey, &((*parent)->left), data);
    }

    /** case 2: newKey is larger (in terms of ASCII table) than parent's key */
    else if(cmpResult >0)
    {
        wasPlaced = placeNode(newKey, &((*parent)->right), data);
    }

    /** case 3: both keys are equal */
    else
    {
        return false;
    }
    if(!wasPlaced)
    {
        return false;
    }
    /** height of a node recalibration in case of a changed height */
    int heightL = getHeight((*parent)->left);
    int heightR = getHeight((*parent)->right);
    (*parent)->height = 1+getMax(heightL, heightR);
    /** get the balance of the current node */
    int balance = heightL - heightR;

    /** case 1: unbalanced toward left side */
    if(balance > 1)
    {
        /** left-right turn in case of a zig-zag node structure */
        if(strcmp((*parent)->left->key, newKey) < 0)
        {
            rotateLeft(&(*parent)->left);
            rotateRight(parent);
            return true;
        }
        /** basic right turn */
        rotateRight(parent);
        return true;
    }
    /** case 2: unbalanced toward right side */
    else if(balance < -1)
    {
        /** right-left turn in case of a zig-zag node structure */
        if(strcmp((*parent)->right->key, newKey) > 0)
        {
            rotateRight(&(*parent)->right);
            rotateLeft(parent);
            return true;
        }
        /** basic left turn */
        rotateLeft(parent);
        return true;
    }
    return true;
} 

static int getHeight(const bstNode* node)
{
    if(node == NULL)
    {
        return 0;
    }
    return node->height;
}


static int getMax(int a, int b)
{
    return a < b ? b : a;
}


static bstNode* allocateNode(const char* key, nodeData* data)
{
    bstNode* newNode = malloc(sizeof(bstNode));
    if (newNode == NULL)
    {
        /* (TODO) free up memory */
        exit(1); /* (TODO) change the code in accordance to specification */
    }
    newNode->key = malloc(strlen(key) + 1);
    if (newNode->key == NULL)
    {
        /* (TODO) free up memory */
        free(newNode);
        exit(1); /* (TODO) change the code in accordance to specification */
    }
    strcpy(newNode->key, key);

    /** setting basic parameters in newNode to basic values */
    newNode->height = 1;
    newNode->left = NULL;
    newNode->right = NULL;
    newNode->data = data;
    return newNode;
}
static void rotateLeft(bstNode** parent)
{
    /** copy into temp the ptr of right child of former parent */
    bstNode* temp = (*parent)->right;
    /** give former parent the branch that is smaller than the new parent */
    (*parent)->right = (*parent)->right->left;
    /** put former parent as a left child of his former right child */
    temp->left = *parent;
    /** insert ptr of a new parent in the place of ptr of the former parent */
    *parent = temp;

    /** change height of new parent and its left child (former parent) */
    (*parent)->left->height = 1+getMax(getHeight((*parent)->left->left), getHeight((*parent)->left->right));
    (*parent)->height = 1+getMax(getHeight((*parent)->left), getHeight((*parent)->right));
}
static void rotateRight(bstNode** parent)
{
    /** copy into temp the ptr of left child of former parent */
    bstNode* temp = (*parent)->left;
    /** give former parent the branch that is bigger than the new parent */
    (*parent)->left = (*parent)->left->right;
    /** put former parent as a right child of his former left child */
    temp->right = *parent;
    /** insert ptr of a new parent in the place of ptr of the former parent */
    *parent = temp;

    /** change height of new parent and its right child (former parent) */
    (*parent)->right->height = 1+getMax(getHeight((*parent)->right->left), getHeight((*parent)->right->right));
    (*parent)->height = 1+getMax(getHeight((*parent)->left), getHeight((*parent)->right));
}

nodeData* bstSearch(bstNode* root, const char* key)
{
    if (root == NULL)
    {
        return NULL;
    }
    int cmpResult = strcmp(key, root->key);
    if (cmpResult< 0)
    {
        return bstSearch(root->left, key);
    }
    else if (cmpResult > 0)
    {
        return bstSearch(root->right, key);
    }
    else
    {
        return root->data;
    }
}

void bstDestroy(bstNode **root, dataEraser eraseDataFunc)
{
    /** first check whether given address of ptr root is valid 
     * then if given address is ok it can proceed to check whether it's in existing node
     * and start backtracing to last real node
     */
    if(root == NULL || *root == NULL)
    {
        return;
    }
    /** post order traversal before freeing current node */
    bstDestroy(&(*root)->left, eraseDataFunc);
    bstDestroy(&(*root)->right, eraseDataFunc);

    free((*root)->key);


    eraseDataFunc((*root)->data);

    /** freeing root and assigning value of root ptr to point towards null*/
    free(*root);
    *root = NULL;
}