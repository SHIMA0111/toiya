use toiya::ffi::arrow::read_from_hyper;

fn main() {
    let res = read_from_hyper("/Users/seigooshima/git-private/toiya/src/toiya-hyperapi/src/data/train.hyper", 2000);
    println!("{:?}", res);
}