int lengthOfLongestSubstring(char* s) {
    int len=strlen(s);
    int ans=0;
    int char_count[256]={0};
    for (int i=0,j=0;i<len;i++)
    {
        char_count[s[i]]++;
        while(char_count[s[i]]>1)
        {
            char_count[s[j]]--;
            j++;
        }
        if((i-j+1)>ans)
        {
            ans=i-j+1;
        }
    }

return ans;
    
}