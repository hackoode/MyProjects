/**
 * Note: The returned array must be malloced, assume caller calls free().
 */
bool* kidsWithCandies(int* candies, int candiesSize, int extraCandies, int* returnSize) {
    //find the max size
    bool* CandieTrueORfalse=(bool*)malloc(candiesSize*sizeof(bool));
    int max_sum=0;
    for (int i=0;i<candiesSize;i++)
    {
        if(max_sum<candies[i])
        max_sum=candies[i];

    }
        for (int i=0;i<candiesSize;i++)
    {
        if(candies[i]+extraCandies>=max_sum)
        CandieTrueORfalse[i]=true;
        else
        CandieTrueORfalse[i]=false;

    }
    *returnSize=candiesSize;
    return CandieTrueORfalse;
    
}