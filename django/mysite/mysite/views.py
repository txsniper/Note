from django.http import HttpResponse
def hello(request):
	context = {}
	context['hello'] = 'Hello World'
	return render(request, 'hello.html',context)

