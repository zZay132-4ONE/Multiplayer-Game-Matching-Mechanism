namespace cpp match_service

/** User of the game */
struct User {
    1: i32 id,
    2: string name,
    3: i32 score
}

/** Service: Match users in the game /*
service Match {
    i32 add_user(1: User user, 2: string info),

    i32 remove_user(1: User user, 2: string info)
}
