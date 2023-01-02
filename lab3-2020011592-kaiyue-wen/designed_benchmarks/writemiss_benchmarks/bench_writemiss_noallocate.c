int main(){
    int a[100];
    int sum = 0;
    for(int k = 0; k < 1000; k++){
        for(int j = 0; j < 32;j++){
            a[j] = k;
        }
        for(int j = 32; j < 64;j++){
            sum += a[j];
        }
    }
    return 0;
}