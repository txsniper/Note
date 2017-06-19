from django.http import HttpResponse
def hello(request):
	context = {}
	context['hello'] = 'Hello World'
	return HttpResponse("Hello World")
