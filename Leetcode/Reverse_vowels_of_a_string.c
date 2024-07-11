bool isVowel(char c) {
    switch (c) {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            return true;
        default:
            return false;
    }
}

// Function to reverse the vowels in a string
char* reverseVowels(char* s) {
    int len = strlen(s);
    int left = 0;
    int right = len - 1;
    char temp;

    while (left < right) {
        // Move left pointer until a vowel is found
        while (left < right && !isVowel(s[left])) {
            left++;
        }
        // Move right pointer until a vowel is found
        while (left < right && !isVowel(s[right])) {
            right--;
        }
        // Swap the vowels
        if (left < right) {
            temp = s[left];
            s[left] = s[right];
            s[right] = temp;
            left++;
            right--;
        }
    }

    return s;
}