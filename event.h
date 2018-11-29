#ifndef EVENT_H
#define EVENT_H

#include <string>

using namespace std;

//  EVENTS
enum Event {
    None = 0,
    Login,
    LoginSuccess,
    Message,
    Logoff
};
bool contains(string str, string substr) {
	for(int i=0; i<str.length(); i++) {
		str[i] = tolower(str[i]);
	}
	for(int i=0; i<substr.length(); i++) {
		substr[i] = tolower(substr[i]);
	}
	bool has = str.find(substr) != string::npos;
	//printf("STRING \"%s\" contains SUBSTRING \"%s\"? %d\n", str.c_str(), substr.c_str(), has);
	return has;
}
Event parse_event_from_string(string input) {

    //Login
    if ( contains( input, "->server#login<") ) {
        printf("Login event detected\n");
        return Login;
    }

    //Message
    if ( contains( input, "->server#logoff") || contains( input, "->server#logoff" )) {
        printf("Logoff event detected\n");
        return Logoff;
    }

    //Login Success
    if ( contains( input, "#Success<") ) {
        printf("Login Success event detected\n");
        return LoginSuccess;
    }

    //Logout
    if ( contains( input, "#") ) {
        printf("Message event detected\n");
        return Message;
    }

    printf("No event detected\n");
    return None;
}

int count_char(string str, char c) {
	int occ = 0;
	for(int i=0; i<str.length(); i++) {
		if (str[i] == c)
			occ++;
	}
	return occ;
}
int get_index_of_nth_char(string str, char c, int n, int startIndex = 0) {
	int occurences = -1;
	for(int i=startIndex; i<str.length(); i++) {
		if (str[i] == c) {
			occurences++;

			if (occurences == n)
				return i;
		}
	}
	return -1;
}
string parse_until(string str, char c) {
	int i = get_index_of_nth_char(str, c, 0);
	return str.substr(0, i);
}
string parse_between(string str, char c1, char c2, int i = 0) {
	int i1 = get_index_of_nth_char(str, c1, i) + 1;
	//printf("SUBSTRING found '%c' in \"%s\" at %d\n", c1, str.c_str(), i1);
	int i2 = get_index_of_nth_char(str, c2, 0, i1);
	//printf("SUBSTRING found '%c' in \"%s\" at %d\n", c2, str.c_str(), i2);
	string substr = str.substr(i1, i2-i1);
	//printf("SUBSTRING of \"%s\" from %d to %d is \"%s\"\n", str.c_str(), i1, i2, substr.c_str());
	return substr;
}


#endif