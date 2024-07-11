/**
 * Note: The returned array must be malloced, assume caller calls free().
 */
int* productExceptSelf(int* nums, int numsSize, int* returnSize) {
    int * product=(int*)malloc(sizeof(int)*numsSize);
    int leftproduct=1;
      for (int i=0;i<numsSize;i++)
    {
        product[i]=1;
     
    }
    for (int i=0;i<numsSize;i++)
    {
        product[i]*=leftproduct;
        leftproduct*=nums[i];
    }
    int rightproduct=1;
     for (int i=numsSize-1;i>=0;i--)
    {
        product[i]*=rightproduct;
        rightproduct*=nums[i];
     
    }
    *returnSize=numsSize;
    return product;
    
}