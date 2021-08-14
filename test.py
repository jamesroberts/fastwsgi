import fast_wsgi
print("\n\n========================\n\n")


def test(obj):
    print("TEST OBJECT RECIEVED")
    print("TEST OBJECT VALUE", obj)


print(fast_wsgi.run_server(test))
