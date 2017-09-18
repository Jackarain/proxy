
// javascript auth.


// for socks4 server auth.
var do_auth4 = function(client, name)
{
	var auth = false;

    if (name == "aaa")
		auth = true;

	print("do_auth4, client: ", client, ", name='", name, "'",
        (auth ? ", auth passed." : ", auth fail!"));

	return auth;
};

// for socks5 server auth.
var do_auth = function(client, name, passwd)
{
    var auth = false;

    if (name == "aaa" && passwd == "ccc")
        auth = true;

    print("do_auth, client: ", client, ", name='", name, "'",
        (auth ? ", auth passed." : ", auth fail!"));

    return auth;
};
