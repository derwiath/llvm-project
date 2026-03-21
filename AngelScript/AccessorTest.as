class UAccessortTest
{
	int PublicMember;
	protected int ProtectedMember = 0;
	private int PrivateMember = 0;

	int GetPublicMember() const
	{
		return PublicMember;
	}

	protected int GetProtectedMember() const
	{
		return ProtectedMember;
	}

	private int GetPrivateMember() const
	{
		return PrivateMember;
	}
}
