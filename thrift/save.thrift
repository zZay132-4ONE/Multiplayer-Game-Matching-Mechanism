namespace cpp save_service

service Save {
    # @param username: Name of myserver
    # @param password: Hash value of password of myserver (md5sum)
    # @return: Return 1 if validation passes, else return 0
    # After validation passes, the result will be saved to "myserver:homework/lesson_6/result.txt"
    i32 save_data(1: string username, 2: string password, 3: i32 player1_id, 4: i32 player2_id)
}
