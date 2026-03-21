namespace FormatTest
{

	FString GetString(int Value)
	{
		return f"Value {Value}";
	}

	FString GetLongString()
	{
		return f"Value ================================================================================================================" +
			   f"{Value}";
	}

	FName GetName()
	{
		return n"JohnDoe";
	}
}
