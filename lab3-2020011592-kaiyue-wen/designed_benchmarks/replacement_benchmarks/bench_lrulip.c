int main(){
    int a[100];
    int sum = 0;
    for(int k = 0; k < 1000; k++){
        for(int i = 0; i < 9; i++){
            sum += a[4*i];
        }
    }
    return 0;
}